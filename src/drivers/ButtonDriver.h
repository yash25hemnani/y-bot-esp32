#pragma once
#include <Arduino.h>
#include <app/EventBus.h>

#define LONG_PRESS_MS 1000

class ButtonDriver
{
private:
    volatile uint32_t lastPress = 0;
    EventBus *eventBus;
    uint8_t buttonPin;
    EventType eventType;
    EventType longEventType;
    bool longPressAvailable;
    volatile uint32_t pressStart = 0;

    void IRAM_ATTR handleInterrupt()
    {
        uint32_t now = millis();
        if (digitalRead(buttonPin) == LOW) // pressed
        {
            if (now - lastPress > 200) // debounce
            {
                pressStart = now;
                lastPress = now;
            }
        }
        else // released
        {
            uint32_t heldFor = now - pressStart;
            bool isLongPress = longPressAvailable && heldFor > LONG_PRESS_MS;
            eventBus->publishFromISR(isLongPress ? longEventType : eventType, buttonPin);
        }
    }

    static void IRAM_ATTR isrTrampoline(void *arg)
    {
        static_cast<ButtonDriver *>(arg)->handleInterrupt();
    }

public:
    void init(uint8_t pin, EventBus *bus, EventType type, bool hasLongPress, EventType longType = EventType::PRESS_DETECTED)
    {
        buttonPin = pin;
        eventBus = bus;
        eventType = type;
        longPressAvailable = hasLongPress;
        longEventType = longType;
        pinMode(buttonPin, INPUT_PULLUP);
        attachInterruptArg(digitalPinToInterrupt(buttonPin), isrTrampoline, this, CHANGE);
    }
};