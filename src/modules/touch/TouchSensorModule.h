#pragma once
#include <Arduino.h>
#include <app/EventBus.h>
#include <comms/protocol/Commands.h>

#define STREAK_WINDOW_MS 2000 // max gap between pats to stay in the same streak

class TouchSensorModule {
    private:
        static inline uint32_t lastPat = 0;
        static inline uint32_t patCount = 0;
        static inline EventBus* eventBus;

    public:
        static void init(EventBus* bus) {
            eventBus = bus;
        }

        // From event bus
        static void onEvent(const Event &e) {
            if (e.type != EventType::PAT_DETECTED) return;

            // Streak that works for both A and B
            uint32_t now = millis();
            patCount = (now - lastPat < STREAK_WINDOW_MS) ? patCount + 1 : 1;
            lastPat = now;

            if (patCount == 4) { 
                Serial.println("Patted 4 times.");
                eventBus->publish(EventType::PAT_STREAK_4);
            } else if (patCount == 6) {
                Serial.println("Patted 6 times.");
                eventBus->publish(EventType::PAT_STREAK_6);
                patCount = 0; // streak complete, start fresh
            }
        }

        // No command - only a physical interation
};