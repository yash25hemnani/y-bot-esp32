#pragma once
#include <Arduino.h>

#define DIVIDER_RATIO 2
#define SAMPLE_SIZE 16

class BatteryDriver
{
private:
    uint8_t buttonPin;
    int batteryAvg = 0;

public:
    void init(uint8_t pin)
    {
        buttonPin = pin;
    }

    int readMilliVolts()
    {
        if (!buttonPin)
            return;

        for (int i = 0; i < SAMPLE_SIZE; i++)
        {
            batteryAvg += analogReadMilliVolts(buttonPin);
        }

        batteryAvg /= SAMPLE_SIZE;

        return batteryAvg * DIVIDER_RATIO;
    }
};