#pragma once
#include <Arduino.h>
#include "comms/protocol/Commands.h"
#include "app/Responder.h"

using CommandHandler = void (*)(const Command &);

class CommandRouter
{
private:
    struct HandlerEntry
    {
        char module[16];
        CommandHandler handlerFn;
    };

    static const int MAX_HANDLERS = 8;

    HandlerEntry handlers[MAX_HANDLERS];
    int handlerCount = 0;
    QueueHandle_t cmdQueue;

    void dispatchTask()
    {
        Command cmd;
        for (;;)
        {
            if (xQueueReceive(cmdQueue, &cmd, portMAX_DELAY) != pdTRUE)
                continue;

            bool handled = false;
            for (int i = 0; i < handlerCount; i++)
            {
                if (strcmp(handlers[i].module, cmd.module) == 0)
                {
                    handlers[i].handlerFn(cmd);
                    handled = true;
                    break;
                }
            }

            if (!handled)
                Responder::reply(cmd, "error:unknown_module");
        }
    }

    static void dispatchTaskTrampoline(void *arg)
    {
        static_cast<CommandRouter *>(arg)->dispatchTask();
    }

public:
    void init()
    {
        cmdQueue = xQueueCreate(10, sizeof(Command));
        xTaskCreatePinnedToCore(dispatchTaskTrampoline, "cmd_router", 3072, this, 5, nullptr, 1);
    }

    void registerHandler(const char *moduleName, CommandHandler handler)
    {
        if (handlerCount >= MAX_HANDLERS)
        {
            Serial.printf("CommandRouter: no room for '%s'\n", moduleName);
            return;
        }

        // snprintf always ends with '\0'; strncpy doesn't when the name fills the buffer.
        snprintf(handlers[handlerCount].module, sizeof(handlers[handlerCount].module), "%s", moduleName);
        handlers[handlerCount].handlerFn = handler;
        handlerCount++;
    }

    // Called from WiFi/BLE Task
    bool submit(const Command& cmd) {
        return xQueueSend(cmdQueue, &cmd, pdMS_TO_TICKS(10)) == pdTRUE;
    }
};