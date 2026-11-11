#pragma once
#include <Arduino.h>

class BatteryDriver
{
private:
    static constexpr int DIVIDER_RATIO = 2; // BAT -> 100k -> pin -> 100k -> GND
    static constexpr int SAMPLE_COUNT = 16;

    int pin = -1; // -1 = init() not called yet

public:
    void init(uint8_t adcPin)
    {
        pin = adcPin;
        analogSetPinAttenuation(pin, ADC_11db); // ~0-3.1 V; full cell (4.2 V) / 2 = 2.1 V
    }

    // Battery voltage in mV, or -1 if init() wasn't called.
    int readMilliVolts()
    {
        if (pin < 0)
            return -1;

        uint32_t sum = 0; // local, so it starts at 0 every call
        for (int i = 0; i < SAMPLE_COUNT; i++)
            sum += analogReadMilliVolts(pin);

        return (sum / SAMPLE_COUNT) * DIVIDER_RATIO;
    }
};