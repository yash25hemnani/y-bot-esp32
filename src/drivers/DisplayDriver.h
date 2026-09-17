#pragma once
#include <config/Pins.h>
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <pgmspace.h>
#include <vector>

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

    static inline uint32_t lastBlinkAt = 0;
    static inline int8_t blinkHeight = -1; // -1 = not currently blinking
    static inline int8_t blinkDir = -1;

    static inline uint8_t bootEyeHeight = 0;
    static inline bool bootDone = false;

    static inline bool reactDone = false;
    static inline uint32_t reactStartedAt = 0;
    static inline int reactSize = 0;

    static inline std::vector<String> menuItems = {"Settings", "Info"};
    static inline uint8_t menuIndex = 0;
    static inline uint8_t selectedIndex = 0;

    static inline char msgText[48] = {0};
    static inline uint8_t *imgBuf = nullptr;
    static inline uint16_t imgW = 0, imgH = 0;

    static void drawEyes(int height)
    {
        display.clearDisplay();

        int y = eyeY + (EYE_HEIGHT - height) / 2; // keep eyes vertically centered as they shrink

        display.fillRoundRect(leftEyeX, y, EYE_WIDTH, height, EYE_RADIUS, SSD1306_WHITE);
        display.fillRoundRect(rightEyeX, y, EYE_WIDTH, height, EYE_RADIUS, SSD1306_WHITE);

        display.display();
    }

public:
    // indices into menuItems — keep in sync with the list above
    static constexpr uint8_t MENU_ITEM_SETTINGS = 0;
    static constexpr uint8_t MENU_ITEM_INFO = 1;

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

    static void startBootAnimation()
    {
        bootEyeHeight = 0;
        bootDone = false;
    }

    static void tickBootAnimation(uint32_t startedAt)
    {
        uint32_t elapsed = millis() - startedAt;
        bootEyeHeight = constrain((elapsed * EYE_HEIGHT) / 600, 0, EYE_HEIGHT);
        drawEyes(bootEyeHeight);
        if (bootEyeHeight >= EYE_HEIGHT)
            bootDone = true;
    }

    static bool bootAnimationDone() { return bootDone; }

    static void tickIdleEyes()
    {
        uint32_t now = millis();

        if (blinkHeight < 0)
        {
            drawEyes(EYE_HEIGHT);
            if (now - lastBlinkAt > 2000) // blink every ~2s
            {
                blinkHeight = EYE_HEIGHT;
                blinkDir = -1;
                lastBlinkAt = now;
            }
            return;
        }

        blinkHeight += blinkDir * 4;
        if (blinkHeight <= 2)
        {
            blinkDir = 1; // hit bottom, start reopening
        }
        else if (blinkHeight >= EYE_HEIGHT)
        {
            blinkHeight = -1; // blink finished
            return;
        }
        drawEyes(blinkHeight);
    }

    static void tickReactionAnimation()
    {
        if (reactStartedAt == 0)
        {
            reactStartedAt = millis();
            reactSize = EYE_HEIGHT;
        }

        // grow the two eyes outward each tick until they fill the screen
        reactSize = min(reactSize + 4, 100);

        int w = map(reactSize, 0, 100, EYE_WIDTH, SCREEN_WIDTH / 2);
        int h = map(reactSize, 0, 100, EYE_HEIGHT, SCREEN_HEIGHT);
        int gap = map(reactSize, 0, 100, EYE_GAP, 0);
        int y = (SCREEN_HEIGHT - h) / 2;

        int leftX = SCREEN_WIDTH / 2 - gap / 2 - w;
        int rightX = SCREEN_WIDTH / 2 + gap / 2;

        display.clearDisplay();
        if (reactSize >= 100)
        {
            display.fillRect(leftX, y, w, h, SSD1306_WHITE);
            display.fillRect(rightX, y, w, h, SSD1306_WHITE);
        }
        else
        {
            display.fillRoundRect(leftX, y, w, h, EYE_RADIUS, SSD1306_WHITE);
            display.fillRoundRect(rightX, y, w, h, EYE_RADIUS, SSD1306_WHITE);
        }
        display.display();

        if (reactSize >= 100 && millis() - reactStartedAt > 1200)
        {
            reactDone = true;
            reactStartedAt = 0;
        }
    }

    static bool reactionAnimationDone()
    {
        bool d = reactDone;
        if (d)
            reactDone = false;
        return d;
    }

    // --- Menu ---
    static void tickMenu()
    {
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println("MENU");
        for (uint8_t i = 0; i < menuItems.size(); i++)
        {
            display.print(menuIndex == i ? "> " : "  ");
            display.println(menuItems[i]);
        }

        display.display();
    }

    static void tickSettings()
    {
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println("SETTINGS");
        display.println();
        display.println("Wifi, BLE name,");
        display.println("and pairing info");
        display.println("go here.");
        display.display();
    }

    static void tickInfo()
    {
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.setCursor(0, 0);
        display.println("INFO");
        display.println();
        display.println("Firmware version,");
        display.println("uptime, etc.");
        display.display();
    }

    static void updateMenuIndex(bool up)
    {
        if (up && menuIndex > 0)
        {
            menuIndex--;
        }
        else if (!up && menuIndex < menuItems.size() - 1)
        {
            menuIndex++;
        }
    }

    static uint8_t selectMenuIndex()
    {
        selectedIndex = menuIndex;
        return selectedIndex;
    }

    // --- Content: text message or image ---
    static void setMessage(const char *text)
    {
        strncpy(msgText, text, sizeof(msgText) - 1);
        msgText[sizeof(msgText) - 1] = '\0';
        if (imgBuf)
        {
            free(imgBuf);
            imgBuf = nullptr;
        }
    }

    static void setImage(uint8_t *buf, uint16_t w, uint16_t h)
    {
        if (imgBuf)
            free(imgBuf);
        imgBuf = buf;
        imgW = w;
        imgH = h;
        msgText[0] = '\0';
    }

    static void tickContent()
    {
        display.clearDisplay();
        if (imgBuf)
        {
            display.drawBitmap(0, 0, imgBuf, imgW, imgH, SSD1306_WHITE);
        }
        else
        {
            display.setTextSize(1);
            display.setTextColor(SSD1306_WHITE);
            display.setCursor(0, 0);
            display.println(msgText);
        }
        display.display();
    }
};
