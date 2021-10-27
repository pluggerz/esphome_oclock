#pragma once

#include <FastGPIO.h>
#define APA102_USE_FAST_GPIO

#include <APA102.h>

#include "pins.h"

// Define which pins to use.
const uint8_t dataPin = LED_DATA_PIN;
const uint8_t clockPin = LED_CLOCK_PIN;

// Create an object for writing to the LED strip.
APA102<dataPin, clockPin> ledStrip;

// Set the number of LEDs to control.
const int LED_COUNT = 12;

class LedUtil
{
    static int level;

    static void send(int id, const rgb_color &on, const rgb_color &off)
    {
        ledStrip.startFrame();
        for (auto i = 1; i <= LED_COUNT; i++)
        {
            ledStrip.sendColor(i <= id ? on : off, 31);
        }
        ledStrip.endFrame(LED_COUNT);
    }

    static void flicker(int id, const rgb_color &onColor, const rgb_color &offColor)
    {
        int delay_time = 15;
        send(id, onColor, offColor);
        delay(delay_time);
        send(id, rgb_color(0, 0, 0x00), offColor);
        delay(delay_time);
        send(id, onColor, offColor);
        delay(delay_time);
        send(id, rgb_color(0, 0, 0x00), offColor);
        delay(delay_time);
        send(id, onColor, offColor);
    }

public:
    static void reset()
    {
        auto offColor = rgb_color(0xFF, 0x10, 0x10);
        auto onColor = rgb_color(0xFF, 0x00, 0x00);
        flicker(level, onColor, offColor);
        level = -1;
    }
    static void debug(int new_level)
    {
        if (new_level <= level)
            return;
        level = new_level;
        auto offColor = rgb_color(0x10, 0x10, 0x10);
        auto onColor = rgb_color(0, 0, 0xFF);
        flicker(new_level, onColor, offColor);
        delay(60);
    }
};
