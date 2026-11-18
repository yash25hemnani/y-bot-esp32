#pragma once
#include <Arduino.h>

#define BUZZER_RESOLUTION 8

// Plays one frequency at a time on a passive buzzer, or goes silent.
// Knows nothing about melodies; BuzzerModule handles those.
class BuzzerDriver
{
private:
    uint8_t channel;
    bool activeLow;

public:
    // Channel 4 has its own timer (channels share timers in pairs: 0+1, 2+3, 4+5).
    // Changing its frequency for each note leaves the motors' 1 kHz on channels 0-3 alone.
    void init(uint8_t pin, uint8_t ch, bool isActiveLow)
    {
        channel = ch;
        activeLow = isActiveLow;
        ledcSetup(channel, 2000, BUZZER_RESOLUTION);
        stop(); // set the silent level first, so it doesn't blip at boot
        ledcAttachPin(pin, channel);
    }

    void play(uint32_t freqHz)
    {
        if (freqHz == 0)
        {
            stop(); // 0 Hz = a rest between notes
            return;
        }
        ledcSetup(channel, freqHz, BUZZER_RESOLUTION); // new pitch
        ledcWrite(channel, 128);
    }

    void stop()
    {
        // Silence = hold the pin where the transistor stays OFF
        // (MH-FMD is active LOW, so silent = HIGH = 255)
        ledcWrite(channel, activeLow ? 255 : 0);
    }
};