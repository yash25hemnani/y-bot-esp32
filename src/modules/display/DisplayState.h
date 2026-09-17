#pragma once
#include <Arduino.h>

enum class DisplayState
{
    BOOT,
    IDLE,
    REACTION,
    MENU,
    CONTENT,
    SETTINGS,
    INFO,
};

enum class IntentType {
    MENU_PRESS,
    MENU_UP,
    MENU_DOWN,
    MENU_SELECTED,
    PAT_REACTION,
    SHOW_MESSAGE,
    SHOW_IMAGE,
};

struct DisplayIntent {
    IntentType type;
    char text[48];        // SHOW_MESSAGE payload
    uint8_t* imageBuf;    // SHOW_IMAGE payload, heap-owned, freed after render
    uint16_t imgW, imgH;
};