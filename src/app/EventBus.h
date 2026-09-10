#pragma once
#include <Arduino.h>
#include <vector>

// Defining system events
enum class EventType {
    PAT_DETECTED,
};

// Define strucutre of an event
struct Event {
    EventType type;
};

// Function type defination
using EventHandler = void(*)(const Event&);

class EventBus {
    private:
        QueueHandle_t eventQueue;
        std::vector<EventHandler> subscribers;

        void dispatchTask() {
            Event e;

            for(;;) {
                if (xQueueReceive(eventQueue, &e, portMAX_DELAY) == pdTRUE) {
                    for(EventHandler handler: subscribers) {
                        handler(e);
                    }
                }
            }
        }

        static void dispatchTaskTrampoline(void* arg) {
            static_cast<EventBus*>(arg)->dispatchTask();
        }

    public:
        void init() {
            eventQueue = xQueueCreate(10, sizeof(Event));
            xTaskCreatePinnedToCore(dispatchTaskTrampoline, "event_bus", 2048, this, 5, nullptr, 1);
        }

        void subscribe(EventHandler handler) {
            subscribers.push_back(handler);
        }

        void publishFromISR(EventType type) {
            Event e{type};
            BaseType_t woken = pdFALSE;
            xQueueSendFromISR(eventQueue, &e, &woken);

            if (woken) portYIELD_FROM_ISR();
        }

        void publish(EventType type) {
            Event e{type};
            xQueueSend(eventQueue, &e, pdMS_TO_TICKS(10));
        }
};