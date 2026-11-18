#pragma once
#include <Arduino.h>

// Plays one frequency at a time on a passive buzzer, or goes silent.
// Uses only the core's built-in tone() / noTone(). Knows nothing about melodies;
// BuzzerModule handles those.
//
// The MH-FMD module (3-pin: VCC, I/O, GND) is active LOW: its transistor switches
// the coil on while I/O is LOW. So "silent" must be a steady HIGH, never LOW,
// or the coil sits on DC: no sound, but ~200 mA drawn.
class BuzzerDriver
{
private:
    uint8_t pin;
    uint8_t silentLevel;

public:
    void init(uint8_t buzzerPin, uint8_t channel, bool isActiveLow)
    {
        pin = buzzerPin;
        silentLevel = isActiveLow ? HIGH : LOW;
        pinMode(pin, OUTPUT);
        digitalWrite(pin, silentLevel); // silent before the first note

        // tone() uses LEDC channel 0 by default, which is the right motor's channel.
        // Channel 4 has its own timer, so retuning it per note leaves the motors alone.
        setToneChannel(channel);
    }

    void play(uint32_t freqHz)
    {
        if (freqHz == 0)
        {
            stop(); // 0 Hz = a rest between notes
            return;
        }
        tone(pin, freqHz); // no duration: BuzzerModule times each note itself
    }

    void stop()
    {
        // noTone() detaches the pin from the PWM. The pin then falls back to its
        // plain GPIO output level, so keep that level at "silent" (HIGH for MH-FMD).
        digitalWrite(pin, silentLevel);
        noTone(pin);
    }
};
