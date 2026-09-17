#pragma once
#include <Arduino.h>
#include <app/EventBus.h>
#include <comms/protocol/Commands.h>
#include <drivers/DisplayDriver.h>
#include "DisplayState.h"

class DisplayModule
{
private:
    static inline QueueHandle_t intentQueue;
    static inline DisplayState state = DisplayState::BOOT;
    static inline DisplayState returnState = DisplayState::IDLE; // CONTENT and RECTION will fall back here
    static inline uint32_t stateEnteredAt = 0;

    static void enterState(DisplayState st)
    {
        state = st;
        stateEnteredAt = millis();
    }

    static void handleIntent(const DisplayIntent &in)
    {
        switch (in.type)
        {
            // If on menu, go to idle. If not on menu -> go to menu
        case IntentType::MENU_PRESS:
            enterState(state == DisplayState::MENU ? DisplayState::IDLE : DisplayState::MENU);
            break;

        case IntentType::MENU_UP:
            if (state == DisplayState::MENU)
                DisplayDriver::updateMenuIndex(true);
            break;

        case IntentType::MENU_DOWN:
            if (state == DisplayState::MENU)
                DisplayDriver::updateMenuIndex(false);
            break;

        case IntentType::MENU_SELECTED:
            if (state == DisplayState::MENU)
            {
                uint8_t selectedIndex = DisplayDriver::selectMenuIndex();
                switch (selectedIndex)
                {
                case DisplayDriver::MENU_ITEM_SETTINGS:
                    enterState(DisplayState::SETTINGS);
                    break;

                case DisplayDriver::MENU_ITEM_INFO:
                    enterState(DisplayState::INFO);
                    break;
                }
            }

            break;

        case IntentType::PAT_REACTION:
            // If Idle state, enter reaction state
            if (state == DisplayState::IDLE)
                enterState(DisplayState::REACTION);
            break;

        case IntentType::SHOW_MESSAGE:
            // returnState defines which was the last state to move back to it
            returnState = (state == DisplayState::MENU) ? DisplayState::MENU : DisplayState::IDLE;
            // Set message
            DisplayDriver::setMessage(in.text);
            // Enter content state
            enterState(DisplayState::CONTENT);
            break;

        case IntentType::SHOW_IMAGE:
            // returnState defines which was the last state to move back to it
            returnState = (state == DisplayState::MENU) ? DisplayState::MENU : DisplayState::IDLE;
            // Show content
            DisplayDriver::setImage(in.imageBuf, in.imgW, in.imgH); // driver takes ownership, frees when done
            // Enter conent state
            enterState(DisplayState::CONTENT);
            break;
        }
    }

    static void tick()
    {
        switch (state)
        {
        case DisplayState::BOOT:
            DisplayDriver::tickBootAnimation(stateEnteredAt);
            if (DisplayDriver::bootAnimationDone())
                enterState(DisplayState::IDLE);
            break;

        case DisplayState::IDLE:
            DisplayDriver::tickIdleEyes();
            break;

        case DisplayState::REACTION:
            DisplayDriver::tickReactionAnimation();
            if (DisplayDriver::reactionAnimationDone())
                enterState(DisplayState::IDLE);
            break;

        case DisplayState::MENU:
            DisplayDriver::tickMenu();
            break;

        case DisplayState::SETTINGS:
            DisplayDriver::tickSettings();
            if (millis() - stateEnteredAt > 15000)
                enterState(DisplayState::MENU); // auto-timeout
            break;

        case DisplayState::INFO:
            DisplayDriver::tickInfo();
            if (millis() - stateEnteredAt > 15000)
                enterState(DisplayState::MENU); // auto-timeout
            break;

        case DisplayState::CONTENT:
            DisplayDriver::tickContent();
            if (millis() - stateEnteredAt > 5000)
                enterState(returnState); // auto-timeout
            break;
        }
    }

    static void taskLoop(void *)
    {
        DisplayDriver::init();
        DisplayDriver::startBootAnimation();
        stateEnteredAt = millis();

        DisplayIntent in;
        for (;;)
        {
            if (xQueueReceive(intentQueue, &in, pdMS_TO_TICKS(30)) == pdTRUE)
            {
                handleIntent(in);
            }
            tick();
        }
    }

public:
    static void init()
    {
        intentQueue = xQueueCreate(4, sizeof(DisplayIntent));
        xTaskCreatePinnedToCore(taskLoop, "display", 4096, nullptr, 4, nullptr, 1);
    }

    // From Event Bus
    static void onEvent(const Event &e)
    {
        DisplayIntent in{};
        if (e.type == EventType::MENU_PRESSED)
        {
            in.type = IntentType::MENU_PRESS;
        }
        else if (e.type == EventType::PAT_STREAK_6)
        {
            in.type = IntentType::PAT_REACTION;
        }
        else if (e.type == EventType::UP_PRESSED)
        {
            in.type = IntentType::MENU_UP;
        }
        else if (e.type == EventType::DOWN_PRESSED)
        {
            in.type = IntentType::MENU_DOWN;
        }
        else if (e.type == EventType::MENU_LONG_PRESSED)
        {
            in.type = IntentType::MENU_SELECTED;
        }
        else
        {
            return;
        }

        xQueueSend(intentQueue, &in, 0);
    }

    static void showMessage(const char *text)
    {
        DisplayIntent in{};
        in.type = IntentType::SHOW_MESSAGE;
        strncpy(in.text, text, sizeof(in.text) - 1);
        xQueueSend(intentQueue, &in, 0);
    }

    static void showImage(uint8_t *buf, uint16_t w, uint16_t h)
    {
        DisplayIntent in{};
        in.type = IntentType::SHOW_IMAGE;
        in.imageBuf = buf;
        in.imgW = w;
        in.imgH = h;
        xQueueSend(intentQueue, &in, 0);
    }
};
