#pragma once
#include <WiFi.h>
#include <WebServer.h>
#include "app/CommandRouter.h"
#include "comms/protocol/Commands.h"

class WifiManager
{
private:
    WebServer server{80};
    CommandRouter *commandRouter;

    void handleCommand()
    {
        Command cmd{};
        String moduleArg = server.arg("module");
        String actionArg = server.arg("action");

        strncpy(cmd.module, moduleArg.c_str(), sizeof(cmd.module));
        strncpy(cmd.action, actionArg.c_str(), sizeof(cmd.action));

        commandRouter->submit(cmd);

        server.send(200, "text/plain", "queued: " + moduleArg + "/" + actionArg);
    }

    void taskLoop()
    {
        for (;;)
        {
            server.handleClient();
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    static void taskTrampoline(void *arg)
    {
        static_cast<WifiManager *>(arg)->taskLoop();
    }

public:
    void init(const char *ssid, const char *password, CommandRouter *router)
    {
        commandRouter = router;
        WiFi.begin(ssid, password);
        Serial.print("Connecting to WiFi");

        while (WiFi.status() != WL_CONNECTED)
        {
            delay(300);
            Serial.print(".");
        }
        Serial.println();
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());

        server.on("/cmd", HTTP_GET, [this]()
                  { handleCommand(); });
        server.begin();

        // WifiManager gets its onw task so server.handleClient() never blocks anything else
        xTaskCreatePinnedToCore(taskTrampoline, "wifi_mgr", 4096, this, 4, nullptr, 0);
    }
};