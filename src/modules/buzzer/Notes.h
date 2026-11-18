#pragma once
#include <Arduino.h>

// Note frequencies in Hz (rounded). FS = F sharp, CS = C sharp.
#define NOTE_REST 0
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_FS4 370
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_CS5 554
#define NOTE_D5  587
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_G5  784
#define NOTE_A5  880

// Small buzzers are loudest somewhere around 2-4 kHz. Tune this by ear.
#define NOTE_FIND 2700

struct Note
{
    uint16_t freq; // Hz, 0 = rest
    uint16_t ms;   // total length, including the short gap at the end
};
