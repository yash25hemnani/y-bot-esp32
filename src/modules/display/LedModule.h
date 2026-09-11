#pragma once
#include <Arduino.h>
#include <app/EventBus.h>
#include <comms/protocol/Commands.h>

class LedModule
{
private:
    static inline bool ledState = false;
    static inline uint8_t ledPin;

    static void setState(bool on)
    {
        ledState = on;
        digitalWrite(ledPin, ledState);
        Serial.println(ledState ? "LED ON" : "LED OFF");
    }

    static void toggle()
    {
        setState(!ledState);
    }

public:
    static void init(uint8_t pin)
    {
        ledPin = pin;
        pinMode(ledPin, OUTPUT);
        digitalWrite(ledPin, LOW);
    }

    // From event bus
    static void onEvent(const Event &e)
    {
        if (e.type == EventType::PRESS_DETECTED)
        {
            toggle();
        }
    }

    // From command router
    static void onCommand(const Command &cmd)
    {
        if (strcmp(cmd.action, "toggle") == 0)
            toggle();
        else if (strcmp(cmd.action, "on") == 0)
            setState(true);
        else if (strcmp(cmd.action, "off") == 0)
            setState(false);
    }
};