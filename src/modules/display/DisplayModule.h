#pragma once
#include <Arduino.h>
#include <app/EventBus.h>
#include <comms/protocol/Commands.h>
#include <drivers/DisplayDriver.h>

class DisplayModule
{
public:
    static void init()
    {
        DisplayDriver::init();

        DisplayDriver::drawEyes(EYE_HEIGHT);

        while (true)
        {
            delay(1000);

            DisplayDriver::blink();

            delay(1000);
        }
    }
};
