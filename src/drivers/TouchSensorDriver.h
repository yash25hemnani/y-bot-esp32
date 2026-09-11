#pragma once
#include <Arduino.h>
#include <app/EventBus.h>

#define DEBOUNCE_MS 200

class TouchSensorDriver {
    private:
        static inline volatile uint32_t lastTouch = 0;
        static inline EventBus* eventBus;
        static inline uint8_t touchPin;

        static void IRAM_ATTR isr(void* arg) {
            uint32_t now = millis();

            if (now - lastTouch > DEBOUNCE_MS) { // reject bounce
                lastTouch = now;
                eventBus->publishFromISR(EventType::PAT_DETECTED, touchPin);
            }
        }

    public:
        static void init(uint8_t pin, EventBus* bus) {
            touchPin = pin;
            eventBus = bus;
            pinMode(touchPin, INPUT_PULLDOWN); // HIGH = Pressed
            attachInterruptArg(digitalPinToInterrupt(touchPin), isr, nullptr, RISING);
        }
};