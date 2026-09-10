#pragma once
#include <Arduino.h>
#include "comms/protocol/Commands.h"

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
            if (xQueueReceive(cmdQueue, &cmd, portMAX_DELAY) == pdTRUE)
            {
                for (int i = 0; i < handlerCount; i++)
                {
                    if (strcmp(handlers[i].module, cmd.module) == 0)
                    {
                        handlers[i].handlerFn(cmd);
                        break;
                    }
                }
            }
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
        if (handlerCount < MAX_HANDLERS)
        {
            strncpy(handlers[handlerCount].module, moduleName, sizeof(handlers[handlerCount].module));

            handlers[handlerCount].handlerFn = handler;
            handlerCount++;
        }
    }

    // Called from WiFi/BLE Task
    void submit(const Command& cmd) {
        xQueueSend(cmdQueue, &cmd, pdMS_TO_TICKS(10));
    }
};