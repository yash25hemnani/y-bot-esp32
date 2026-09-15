#pragma once
#include <config/Pins.h>
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <pgmspace.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// Eye geometry
#define EYE_WIDTH 20
#define EYE_HEIGHT 30
#define EYE_RADIUS 8
#define EYE_GAP 20 // space between eyes

int leftEyeX = SCREEN_WIDTH / 2 - EYE_GAP / 2 - EYE_WIDTH;
int rightEyeX = SCREEN_WIDTH / 2 + EYE_GAP / 2;
int eyeY = (SCREEN_HEIGHT - EYE_HEIGHT) / 2;

class DisplayDriver
{
private:
    static inline Adafruit_SSD1306 display{SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET};

public:
    static void init()
    {
        if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
        {
            Serial.println("SSD1306 allocation failed");
            while (true)
                ;
        }

        display.clearDisplay();
        display.display();
    }

    static void print(const char *text)
    {
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println(text);
        display.display();
    }

    static void drawEyes(int height)
    {
        display.clearDisplay();

        int y = eyeY + (EYE_HEIGHT - height) / 2; // keep eyes vertically centered as they shrink

        display.fillRoundRect(leftEyeX, y, EYE_WIDTH, height, EYE_RADIUS, SSD1306_WHITE);
        display.fillRoundRect(rightEyeX, y, EYE_WIDTH, height, EYE_RADIUS, SSD1306_WHITE);

        display.display();
    }

    static void blink()
    {
        // Close
        for (int h = EYE_HEIGHT; h >= 2; h -= 4)
        {
            drawEyes(h);
            delay(15);
        }
        // Open
        for (int h = 2; h <= EYE_HEIGHT; h += 4)
        {
            drawEyes(h);
            delay(15);
        }
    }
};
