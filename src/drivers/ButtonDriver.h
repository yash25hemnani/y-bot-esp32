#pragma once
#include <Arduino.h>
#include <app/EventBus.h>

class ButtonDriver {
    private:
        static inline volatile uint32_t lastPress = 0;
        static inline EventBus* eventBus;
        static inline uint8_t buttonPin;

        static void IRAM_ATTR isr() {
            uint32_t now = millis();
            
            if (now - lastPress > 200) {
                lastPress = now;
                eventBus->publishFromISR(EventType::PRESS_DETECTED);
            }

        }

    public:
        static void init(uint8_t pin, EventBus *bus) {
            buttonPin = pin;
            eventBus = bus;
            pinMode(buttonPin, INPUT_PULLUP); // HIGH = No Button Press
            attachInterrupt(digitalPinToInterrupt(buttonPin), isr, FALLING);
        }
};