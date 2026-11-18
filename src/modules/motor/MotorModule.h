#pragma once
#include <Arduino.h>
#include <comms/protocol/Commands.h>
#include "app/Responder.h"
#include <config/Pins.h>
#include <drivers/MotorDriver.h>

// Motor control with soft start.
//
// Commands only set a target speed per wheel. A single control task steps the
// real PWM duty toward that target, so the motors never jump from 0 to full
// power (or straight from forward to reverse). That start-up current spike is
// what trips the battery module's overcurrent protection and resets the ESP32.
//
// Only the control task writes to the motor drivers, so the command path and
// the ramp never fight over the PWM pins.
class MotorModule
{
private:
    static constexpr uint32_t TICK_MS = 10;
    // Duty change per tick: 0 -> 255 takes ~260 ms. Raise for snappier
    // response, lower if the supply still cuts out.
    static constexpr int RAMP_STEP = 10;
    // Coast if no command arrives for this long (the app resends every 200 ms)
    static constexpr uint32_t WATCHDOG_MS = 500;
    // Top PWM duty. 153 = 60% of 255: less start-up current so the battery
    // module doesn't cut out. 100% from the app now means this value.
    static constexpr int MAX_DUTY = 153; // 60% (80% = 204 trips the module, 40% = 102 too weak for the yellow motor)

    static inline MotorDriver rightMotorDriver;
    static inline MotorDriver leftMotorDriver;

    // Written by the command router, read by the control task
    static inline volatile uint32_t lastCmdMs = 0;
    static inline volatile int targetLeft = 0;
    static inline volatile int targetRight = 0;
    static inline volatile bool brakeRequested = false;

    // Owned by the control task
    static inline int currentLeft = 0;
    static inline int currentRight = 0;
    static inline bool idle = true; // coasting or braked: nothing to write until a new target

    static void setTargets(int left, int right)
    {
        targetLeft = left;
        targetRight = right;
    }

    static int stepToward(int current, int target)
    {
        if (current < target)
            return min(current + RAMP_STEP, target);
        if (current > target)
            return max(current - RAMP_STEP, target);
        return current;
    }

    static void controlTask(void *arg)
    {
        bool stopped = true; // only for logging, so we print once per stop
        for (;;)
        {
            if (millis() - lastCmdMs > WATCHDOG_MS)
            {
                // Lost contact: stop driving right away (coasting draws no current)
                targetLeft = targetRight = 0;
                if (!idle)
                {
                    currentLeft = currentRight = 0;
                    rightMotorDriver.coast();
                    leftMotorDriver.coast();
                    idle = true;
                }
                if (!stopped)
                {
                    Serial.println("[Motor] watchdog: no command for 500 ms, coasting");
                    stopped = true;
                }
            }
            else
            {
                stopped = false;

                if (brakeRequested)
                {
                    brakeRequested = false;
                    currentLeft = currentRight = 0;
                    rightMotorDriver.brake();
                    leftMotorDriver.brake();
                    idle = true;
                }
                else
                {
                    int nextLeft = stepToward(currentLeft, targetLeft);
                    int nextRight = stepToward(currentRight, targetRight);

                    if (nextLeft != currentLeft || nextRight != currentRight)
                    {
                        currentLeft = nextLeft;
                        currentRight = nextRight;
                        leftMotorDriver.drive(currentLeft);
                        rightMotorDriver.drive(currentRight);
                        idle = (currentLeft == 0 && currentRight == 0);
                    }
                }
            }

            vTaskDelay(pdMS_TO_TICKS(TICK_MS));
        }
    }

public:
    static void init()
    {
        rightMotorDriver.init(RIGHT_MOTOR_A_PIN, RIGHT_MOTOR_B_PIN, 0, 1);
        leftMotorDriver.init(LEFT_MOTOR_A_PIN, LEFT_MOTOR_B_PIN, 2, 3);
        xTaskCreatePinnedToCore(controlTask, "motor_ctrl", 2048, nullptr, 6, nullptr, 1);
    }

    // From command router
    static void onCommand(const Command &cmd)
    {
        int percent = constrain(atoi(cmd.value), 0, 100);
        int speed = map(percent, 0, 100, 0, MAX_DUTY);
        Serial.printf("[Motor] %s at %d%% (PWM %d)\n", cmd.action, percent, speed);

        if (strcmp(cmd.action, "forward") == 0)
            setTargets(speed, speed);
        else if (strcmp(cmd.action, "backward") == 0)
            setTargets(-speed, -speed);
        else if (strcmp(cmd.action, "left") == 0)
            setTargets(0, speed); // right wheel only, as before
        else if (strcmp(cmd.action, "right") == 0)
            setTargets(speed, 0); // left wheel only, as before
        else if (strcmp(cmd.action, "brake") == 0)
        {
            // Braking shorts the motor windings instead of drawing from the
            // battery, so it stays instant
            setTargets(0, 0);
            brakeRequested = true;
        }
        else
        {
            Responder::reply(cmd, "error:unknown_action");
            return;
        }

        lastCmdMs = millis();
    }
};
