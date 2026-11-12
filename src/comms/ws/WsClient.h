#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <atomic>
#include "config/Server.h"
#include "app/CommandRouter.h"
#include "comms/protocol/CommandParser.h"
#include "comms/ws/Hmac.h"
#include "modules/settings/SettingsModule.h"

// Login flow:
//   DISCONNECTED -> (socket opens) -> WAIT_CHALLENGE -> (we send auth:login) -> WAIT_AUTH_OK -> ONLINE
// Any close sends us back to DISCONNECTED; the library reconnects by itself.
enum class WsState : uint8_t
{
    DISCONNECTED,
    WAIT_CHALLENGE,
    WAIT_AUTH_OK,
    ONLINE,
};

// One outgoing message. Fixed-size so the queue can copy it by value (String would copy a heap pointer).
struct OutMsg
{
    char text[128];
};

class WsClient
{
private:
    // The WebSockets library is not thread-safe: every ws.* call happens on the ws_client task only.
    WebSocketsClient ws;
    CommandRouter *commandRouter = nullptr;

    String deviceId;
    String hmacSecret; // never logged
    bool started = false;
    bool missingCredsLogged = false;

    static inline std::atomic<WsState> state{WsState::DISCONNECTED};

    static const unsigned long RECONNECT_MS = 5000;
    static const unsigned long AUTH_FAIL_RETRY_MS = 30000; // slow down if our keys are wrong
    static const size_t MAX_MESSAGE = 512;
    static const size_t OUTBOX_DEPTH = 8;

    // Replies from other tasks wait here; only the ws_client task sends them.
    QueueHandle_t outbox = nullptr;

    void sendText(const String &msg)
    {
        ws.sendTXT(msg.c_str());
    }

    // Runs on the ws_client task only (it calls ws.*).
    void drainOutbox()
    {
        OutMsg m;
        while (xQueueReceive(outbox, &m, 0) == pdTRUE)
            sendText(m.text);
    }

    // WiFi is up: load keys and start connecting. Keys are re-read every time,
    // so new values written over BLE are picked up on the next connect.
    void start()
    {
        deviceId = SettingsModule::loadKey("device_id");
        hmacSecret = SettingsModule::loadKey("hmac_secret");

        if (deviceId.isEmpty() || hmacSecret.isEmpty())
        {
            if (!missingCredsLogged)
                Serial.println("WS: device_id or hmac_secret not set, not connecting");
            missingCredsLogged = true;
            return;
        }
        missingCredsLogged = false;

        // TODO: switch to beginSslWithCA so we also check WHO answers, not just encrypt.
        if (WS_USE_TLS)
            ws.beginSSL(WS_HOST, WS_PORT, WS_PATH);
        else
            ws.begin(WS_HOST, WS_PORT, WS_PATH);
        ws.setReconnectInterval(RECONNECT_MS);
        ws.enableHeartbeat(15000, 3000, 2); // we also ping, so we notice a dead server
        started = true;
        Serial.printf("WS: connecting to %s://%s:%u%s\n", WS_USE_TLS ? "wss" : "ws", WS_HOST, WS_PORT, WS_PATH);
    }

    void stop()
    {
        ws.disconnect();
        started = false;
        state = WsState::DISCONNECTED;
        xQueueReset(outbox); // old replies are useless after a reconnect
        Serial.println("WS: WiFi lost, stopped");
    }

    void handleChallenge(const String &text)
    {
        const String prefix = "auth:challenge:";
        if (!text.startsWith(prefix))
            return; // ignore anything else before login

        String nonce = text.substring(prefix.length());
        if (nonce.isEmpty() || nonce.length() > 64)
        {
            ws.disconnect();
            return;
        }

        String sig = hmacSha256Hex(hmacSecret, deviceId + ":" + nonce);
        if (sig.isEmpty())
        {
            Serial.println("WS: HMAC failed");
            ws.disconnect();
            return;
        }

        sendText("auth:login:" + deviceId + ":" + sig);
        state = WsState::WAIT_AUTH_OK;
        Serial.println("WS: login sent"); // never log the signature or secret
    }

    void handleAuthReply(const String &text)
    {
        if (text == "auth:ok")
        {
            state = WsState::ONLINE;
            ws.setReconnectInterval(RECONNECT_MS);
            Serial.println("WS: online");
        }
        else if (text == "auth:fail")
        {
            // Server closes the socket next. Retry slowly: wrong keys won't fix themselves.
            ws.setReconnectInterval(AUTH_FAIL_RETRY_MS);
            Serial.println("WS: auth failed (check device_id / hmac_secret)");
        }
    }

    void handleCommand(const String &text)
    {
        if (text.startsWith("auth:"))
            return; // no auth messages expected once online

        Command cmd;
        ParseResult r = CommandParser::parse(text, cmd);
        if (r != ParseResult::OK)
        {
            sendText(CommandParser::errorText(r));
            return;
        }

        // Keys and WiFi credentials can only be changed over BLE (someone next to the robot).
        if (strcmp(cmd.module, "settings") == 0)
        {
            sendText("error:forbidden");
            return;
        }

        cmd.source = CommandSource::WS;
        if (!commandRouter->submit(cmd))
        {
            sendText("error:busy");
            return;
        }

        sendText("ok:queued");
        Serial.printf("WS queued: %s/%s/%s\n", cmd.module, cmd.action, cmd.key); // never log value
    }

    void handleText(const String &text)
    {
        switch (state.load())
        {
        case WsState::WAIT_CHALLENGE:
            handleChallenge(text);
            break;
        case WsState::WAIT_AUTH_OK:
            handleAuthReply(text);
            break;
        case WsState::ONLINE:
            handleCommand(text);
            break;
        default:
            break;
        }
    }

    // Called from inside ws.loop(), so it runs on the ws_client task.
    void onEvent(WStype_t type, uint8_t *payload, size_t length)
    {
        switch (type)
        {
        case WStype_CONNECTED:
            state = WsState::WAIT_CHALLENGE;
            Serial.println("WS: connected, waiting for challenge");
            break;

        case WStype_DISCONNECTED:
            // Also fires on every failed connect attempt; only log a real drop.
            if (state != WsState::DISCONNECTED)
                Serial.println("WS: disconnected");
            state = WsState::DISCONNECTED;
            xQueueReset(outbox); // old replies are useless after a reconnect
            break;

        case WStype_TEXT:
            if (length > MAX_MESSAGE)
                return;
            handleText(String((const char *)payload)); // library null-terminates text frames
            break;

        default:
            break; // binary, ping, pong: nothing to do
        }
    }

    void taskLoop()
    {
        for (;;)
        {
            bool wifiUp = WiFi.status() == WL_CONNECTED;

            if (wifiUp && !started)
                start();
            else if (!wifiUp && started)
                stop();

            if (started)
            {
                ws.loop();
                if (state == WsState::ONLINE)
                    drainOutbox();
            }

            vTaskDelay(pdMS_TO_TICKS(started ? 10 : 500));
        }
    }

    static void taskTrampoline(void *arg)
    {
        static_cast<WsClient *>(arg)->taskLoop();
    }

public:
    void init(CommandRouter *router)
    {
        commandRouter = router;
        outbox = xQueueCreate(OUTBOX_DEPTH, sizeof(OutMsg)); // before the task, so it never sees a null queue
        ws.onEvent([this](WStype_t type, uint8_t *payload, size_t length)
                   { onEvent(type, payload, length); });

        // Own task, same core as wifi_mgr. 10 KB: the TLS handshake (mbedTLS) runs here and needs a deep stack.
        xTaskCreatePinnedToCore(taskTrampoline, "ws_client", 10240, this, 3, nullptr, 0);
    }

    // Safe from any task. Never blocks. Returns false if the message was dropped.
    bool enqueue(const char *msg)
    {
        if (outbox == nullptr)
            return false;

        OutMsg m;
        snprintf(m.text, sizeof(m.text), "%s", msg); // always '\0'-terminated, cuts if too long

        if (xQueueSend(outbox, &m, 0) != pdTRUE)
        {
            Serial.println("WS: outbox full, dropped"); // never log the payload
            return false;
        }
        return true;
    }

    static WsState getState() { return state; }
};
