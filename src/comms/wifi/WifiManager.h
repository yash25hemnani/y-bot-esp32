#pragma once
#include <WiFi.h>
#include <WebServer.h>
#include <atomic>
#include "app/CommandRouter.h"
#include "comms/protocol/Commands.h"
#include "modules/settings/SettingsModule.h"

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
    WebServer server{80};
    CommandRouter *commandRouter;
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

    void reloadCredentials()
    {
        WiFi.disconnect();

        credentialsAvailable = SettingsModule::wifiCredentialAvailable();

        if (!credentialsAvailable)
        {
            Serial.println("No wifi credentials available.");
            return;
        }

        ssid = SettingsModule::loadKey("wifi_name");
        password = SettingsModule::loadKey("wifi_password");

        vTaskDelay(pdMS_TO_TICKS(100)); // let the disconnect settle
        WiFi.begin(ssid.c_str(), password.c_str());
        lastConnectAttempt = millis();

        // SSID only - never the password.
        Serial.printf("Connecting to WiFi: %s\n", ssid.c_str());
    }

    void handleCommand()
    {
        String moduleArg = server.arg("module");
        String actionArg = server.arg("action");
        String keyArg = server.arg("key");
        String valueArg = server.arg("value");

        Command cmd{};
        strncpy(cmd.module, moduleArg.c_str(), sizeof(cmd.module) - 1);
        strncpy(cmd.action, actionArg.c_str(), sizeof(cmd.action) - 1);
        strncpy(cmd.key, keyArg.c_str(), sizeof(cmd.key) - 1);
        strncpy(cmd.value, valueArg.c_str(), sizeof(cmd.value) - 1);

        commandRouter->submit(cmd);

        server.send(200, "text/plain", "queued: " + moduleArg + "/" + actionArg);
    }

    void taskLoop()
    {
        for (;;)
        {
            if (reloadRequested.exchange(false))
            {
                reloadCredentials();
            }

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

            server.handleClient();
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    static void taskTrampoline(void *arg)
    {
        static_cast<WifiManager *>(arg)->taskLoop();
    }

public:
    void init(CommandRouter *router)
    {
        commandRouter = router;

        WiFi.mode(WIFI_STA); // must come before reloadCredentials()
        reloadCredentials();

        server.on("/cmd", HTTP_POST, [this]()
                  { handleCommand(); });
        server.begin();

        // WifiManager gets its own task so connecting/reconnecting never blocks setup() or anything else
        xTaskCreatePinnedToCore(taskTrampoline, "wifi_mgr", 4096, this, 4, nullptr, 0);
    }

    // Runs on the cmd_router task (core 1). Only sets the flag; wifi_mgr (core 0) does the WiFi work.
    static void onCommand(const Command &cmd)
    {
        if (strcmp(cmd.action, "reconnect") == 0)
            reloadRequested = true;
    }
};