#pragma once
#include <Arduino.h>

#define MOTOR_FREQ_HZ 1000
#define MOTOR_RESOLUTION 8

class MotorDriver
{
private:
    uint8_t channelA;
    uint8_t channelB;

public:
    void init(uint8_t pinA, uint8_t pinB, uint8_t chA, uint8_t chB)
    {
        // Setup for Channel A
        channelA = chA;
        ledcSetup(chA, MOTOR_FREQ_HZ, MOTOR_RESOLUTION);
        ledcWrite(chA, 0);
        ledcAttachPin(pinA, chA);

        // Setup for Channel B
        channelB = chB;
        ledcSetup(chB, MOTOR_FREQ_HZ, MOTOR_RESOLUTION);
        ledcWrite(chB, 0);
        ledcAttachPin(pinB, chB);
    }

    void coast()
    {
        ledcWrite(channelA, 0);
        ledcWrite(channelB, 0);
    }

    void brake()
    {
        ledcWrite(channelA, 255);
        ledcWrite(channelB, 255);
    }

    void drive(int speed)
    {
        speed = constrain(speed, -255, 255);
        if (speed > 0)
        {
            // Moves forward
            ledcWrite(channelB, 0);
            ledcWrite(channelA, speed);
        } else if (speed < 0) {
            // Moves backwards
            ledcWrite(channelA, 0);
            ledcWrite(channelB, abs(speed));
        } else {
            coast();
        }
    }
};