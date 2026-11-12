#pragma once
#include <WiFi.h>
#include <atomic>
#include "app/Responder.h"
#include "comms/protocol/Commands.h"
#include "modules/settings/SettingsModule.h"
#include "comms/ble/BleManager.h"

enum class WifiState : uint8_t
{
    NO_CREDENTIALS,
    CONNECTING,
    CONNECTED,
    WRONG_PASSWORD,
    NO_NETWORK,
};

class WifiManager
{
private:
    String ssid;
    String password;
    bool credentialsAvailable = false;
    unsigned long lastConnectAttempt = 0;
    static const unsigned long RECONNECT_INTERVAL_MS = 5000;
    static inline std::atomic<bool> reloadRequested{false};

    BleManager *bleManager = nullptr;
    static inline std::atomic<WifiState> state{WifiState::NO_CREDENTIALS};
    static inline std::atomic<bool> statusPending{false}; // true = send status over BLE
    static inline std::atomic<uint8_t> lastReason{0};     // last disconnect reason, for logs

    static void setState(WifiState s)
    {
        if (state.exchange(s) != s)
        {
            statusPending = true;
        }
    }

    static WifiState stateFromReason(uint8_t reason)
    {
        switch (reason)
        {
        case WIFI_REASON_AUTH_FAIL:
        case WIFI_REASON_AUTH_EXPIRE:
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_HANDSHAKE_TIMEOUT:
            return WifiState::WRONG_PASSWORD;
        case WIFI_REASON_NO_AP_FOUND:
            return WifiState::NO_NETWORK;
        default:
            return WifiState::CONNECTING; // something else; the retry loop keeps trying
        }
    }

    // Runs on Arduino's WiFi event task. Keep it short: only update atomics.
    static void onWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info)
    {
        if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP)
        {
            setState(WifiState::CONNECTED);
        }
        else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED)
        {
            if (state == WifiState::NO_CREDENTIALS)
            {
                return; // we disconnected on purpose
            }

            lastReason = info.wifi_sta_disconnected.reason;
            setState(stateFromReason(lastReason));
        }
    }

    static const char *stateName(WifiState s)
    {
        switch (s)
        {
        case WifiState::NO_CREDENTIALS:
            return "no_credentials";
        case WifiState::CONNECTING:
            return "connecting";
        case WifiState::CONNECTED:
            return "connected";
        case WifiState::WRONG_PASSWORD:
            return "wrong_password";
        case WifiState::NO_NETWORK:
            return "no_network";
        }
        return "unknown";
    }

    // Runs on wifi_mgr only.
    void sendStatus()
    {
        WifiState s = state;
        String msg = "wifi:status:" + String(stateName(s));
        if (s == WifiState::CONNECTED)
            msg += ":" + WiFi.localIP().toString();

        Serial.printf("WiFi status: %s (last reason %u)\n", stateName(s), lastReason.load());
        
        if (bleManager != nullptr)
            bleManager->sendResponse(msg);
    }

    void reloadCredentials()
    {
        WiFi.disconnect();

        credentialsAvailable = SettingsModule::wifiCredentialAvailable();

        if (!credentialsAvailable)
        {
            setState(WifiState::NO_CREDENTIALS);
            Serial.println("No wifi credentials available.");
            return;
        }

        ssid = SettingsModule::loadKey("wifi_name");
        password = SettingsModule::loadKey("wifi_password");

        vTaskDelay(pdMS_TO_TICKS(100)); // let the disconnect settle
        setState(WifiState::CONNECTING);
        WiFi.begin(ssid.c_str(), password.c_str());
        lastConnectAttempt = millis();

        // SSID only - never the password.
        Serial.printf("Connecting to WiFi: %s\n", ssid.c_str());
    }

    void taskLoop()
    {
        for (;;)
        {
            if (reloadRequested.exchange(false))
            {
                reloadCredentials();
            }

            if (statusPending.exchange(false))
                sendStatus();

            if (credentialsAvailable && WiFi.status() != WL_CONNECTED)
            {
                unsigned long now = millis();
                if (now - lastConnectAttempt >= RECONNECT_INTERVAL_MS)
                {
                    lastConnectAttempt = now;
                    Serial.println("WiFi not connected, retrying...");
                    WiFi.begin(ssid.c_str(), password.c_str());
                }
            }

            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    static void taskTrampoline(void *arg)
    {
        static_cast<WifiManager *>(arg)->taskLoop();
    }

public:
    void init(BleManager *ble)
    {
        bleManager = ble;

        WiFi.onEvent(onWifiEvent); // before WiFi.mode, so we don't miss early events
        WiFi.mode(WIFI_STA); // must come before reloadCredentials()
        reloadCredentials();

        // WifiManager gets its own task so connecting/reconnecting never blocks setup() or anything else
        xTaskCreatePinnedToCore(taskTrampoline, "wifi_mgr", 4096, this, 4, nullptr, 0);
    }

    // Runs on the cmd_router task (core 1). Only sets the flag; wifi_mgr (core 0) does the WiFi work.
    static void onCommand(const Command &cmd)
    {
        if (strcmp(cmd.action, "reconnect") == 0)
            reloadRequested = true;
        else if (strcmp(cmd.action, "status") == 0)
            statusPending = true; // always reply, even if nothing changed
        else
            Responder::reply(cmd, "error:unknown_action");
    }
};