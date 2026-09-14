#pragma once
#include <WiFi.h>
#include <WebServer.h>
#include "app/CommandRouter.h"
#include "comms/protocol/Commands.h"
#include "modules/settings/SettingsModule.h"

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
        credentialsAvailable = SettingsModule::wifiCredentialAvailable();

        WiFi.mode(WIFI_STA);

        if (!credentialsAvailable)
        {
            Serial.println("No wifi credentials available.");
        }
        else
        {
            ssid = SettingsModule::loadKey("wifi_name");
            password = SettingsModule::loadKey("wifi_password");

            WiFi.begin(ssid.c_str(), password.c_str());
            lastConnectAttempt = millis();
            Serial.println("Connecting to WiFi in background...");
        }

        server.on("/cmd", HTTP_POST, [this]()
                  { handleCommand(); });
        server.begin();

        // WifiManager gets its own task so connecting/reconnecting never blocks setup() or anything else
        xTaskCreatePinnedToCore(taskTrampoline, "wifi_mgr", 4096, this, 4, nullptr, 0);
    }
};