#pragma once
#include <Arduino.h>
#include <comms/protocol/Commands.h>
#include <config/Pins.h>
#include <drivers/MotorDriver.h>

class MotorModule
{
private:
    static inline uint32_t lastCmdMs = 0;
    static inline MotorDriver rightMotorDriver;
    static inline MotorDriver leftMotorDriver;

    static void forward(int speed)
    {
        rightMotorDriver.drive(speed);
        leftMotorDriver.drive(speed);
    }

    static void backward(int speed)
    {
        rightMotorDriver.drive(-speed);
        leftMotorDriver.drive(-speed);
    }

    static void turnLeft(int speed)
    {
        rightMotorDriver.drive(speed);
        leftMotorDriver.drive(0);
    }
    static void turnRight(int speed)
    {

        leftMotorDriver.drive(speed);
        rightMotorDriver.drive(0);
    }

    static void brake()
    {
        rightMotorDriver.brake();
        leftMotorDriver.brake();
    }

    static void watchdog(void *arg) {
        for (;;)
        {
            if (millis() - lastCmdMs > 500) {
                rightMotorDriver.coast();
                leftMotorDriver.coast();
            }
    
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        
    }

public:
    static void init()
    {
        rightMotorDriver.init(RIGHT_MOTOR_A_PIN, RIGHT_MOTOR_B_PIN, 0, 1);
        leftMotorDriver.init(LEFT_MOTOR_A_PIN, LEFT_MOTOR_B_PIN, 2, 3);
        xTaskCreatePinnedToCore(watchdog, "watchdog", 2048, nullptr, 6, nullptr, 1);
    }

    // From command router
    static void onCommand(const Command &cmd)
    {
        
        int percent = constrain(atoi(cmd.value), 0, 100);
        int speed = map(percent, 0, 100, 0, 255);
        lastCmdMs = millis();

        if (strcmp(cmd.action, "forward") == 0)
            forward(speed);
        else if (strcmp(cmd.action, "backward") == 0)
            backward(speed);
        else if (strcmp(cmd.action, "left") == 0)
            turnLeft(speed);
        else if (strcmp(cmd.action, "right") == 0)
            turnRight(speed);
        else if (strcmp(cmd.action, "brake") == 0)
            brake();
    }
};