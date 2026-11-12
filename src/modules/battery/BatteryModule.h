#pragma once
#include <Arduino.h>
#include <atomic>
#include "app/EventBus.h"
#include "app/Responder.h"
#include "comms/protocol/Commands.h"
#include "drivers/BatteryDriver.h"

class BatteryModule
{
private:
    struct CurvePoint
    {
        int mv;
        int pct;
    };

    // 1S Li-ion, resting voltage -> charge. Highest first. Approximate; tune after real tests.
    static constexpr CurvePoint CURVE[] = {
        {4200, 100},
        {4100, 90},
        {4000, 80},
        {3900, 65},
        {3800, 50},
        {3750, 40},
        {3700, 30},
        {3650, 20},
        {3600, 10},
        {3500, 5},
        {3300, 0},
    };

    static constexpr int CURVE_LEN = sizeof(CURVE) / sizeof(CURVE[0]);

    static constexpr int NO_BATTERY_MV = 2500; // below this, assume nothing is connected
    static constexpr int LOW_ON_PCT = 15;      // fire BATTERY_LOW at or below this
    static constexpr int LOW_OFF_PCT = 20;     // re-arm only after climbing back here
    static constexpr uint32_t READ_INTERVAL_MS = 10000;

    static inline BatteryDriver driver;
    static inline EventBus *eventBus = nullptr;

    // Written by the battery task, read by other tasks.
    static inline std::atomic<int> lastMilliVolts{-1};
    static inline std::atomic<int> lastPercent{-1}; // -1 = no battery / not read yet

    static inline bool lowReported = false; // only the battery task touches this

    static int toPercent(int mv)
    {
        if (mv >= CURVE[0].mv)
        {
            return 100;
        }

        if (mv <= CURVE[CURVE_LEN - 1].mv)
        {
            return 0;
        }

        // Find the two points mv sits between, then draw a straight line between them.
        for (int i = 1; i < CURVE_LEN; i++)
        {
            if (mv >= CURVE[i].mv)
            {
                const CurvePoint &hi = CURVE[i - 1];
                const CurvePoint &lo = CURVE[i];

                return lo.pct + (mv - lo.mv) * (hi.pct - lo.pct) / (hi.mv - lo.mv);
            }
        }

        return 0;
    }

    static void update()
    {
        int mv = driver.readMilliVolts();
        int pct = (mv < NO_BATTERY_MV) ? -1 : toPercent(mv);

        lastMilliVolts = mv;
        lastPercent = pct;
        Serial.printf("Battery: %d mV, %d%%\n", mv, pct);

        if (pct < 0)
            return;

        if (!lowReported && pct <= LOW_ON_PCT)
        {
            lowReported = true;
            if (eventBus != nullptr)
            {
                eventBus->publish(EventType::BATTERY_LOW);
            }
        }
        else if (lowReported && pct >= LOW_OFF_PCT)
        {
            lowReported = false;
        }
    }

    static void taskLoop(void *)
    {
        for (;;)
        {
            update();
            vTaskDelay(pdMS_TO_TICKS(READ_INTERVAL_MS));
        }
    }

public:
    // Call after eventBus.init(): the first reading may publish right away.
    static void init(uint8_t pin, EventBus *bus)
    {
        driver.init(pin);
        eventBus = bus;
        xTaskCreatePinnedToCore(taskLoop, "battery", 3072, nullptr, 2, nullptr, 1);
    }

    static int getPercent() { return lastPercent; }

    // battery:read  ->  battery:read:<pct>:<mv>
    static void onCommand(const Command &cmd)
    {
        if (strcmp(cmd.action, "read") == 0)
            Responder::reply(cmd, "battery:read:" + String(lastPercent.load()) + ":" + String(lastMilliVolts.load()));
        else
            Responder::reply(cmd, "error:unknown_action");
    }
};