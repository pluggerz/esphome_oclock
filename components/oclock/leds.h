#ifdef AVR
#include <FastGPIO.h>
#define APA102_USE_FAST_GPIO
#endif

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
    static void send(int id, const rgb_color &on, const rgb_color &off)
    {
        ledStrip.startFrame();
        for (uint16_t i = 1; i <= LED_COUNT; i++)
        {
            ledStrip.sendColor(i <= id ? rgb_color(0, 0, 0xFF) : rgb_color(0, 0xFF, 0xFF), 31);
        }
        ledStrip.endFrame(LED_COUNT);
    }

    static void flicker(int id)
    {
        send(id, rgb_color(0, 0, 0xFF), rgb_color(0, 0xFF, 0xFF));
        delay(30);
        send(id, rgb_color(0, 0, 0x00), rgb_color(0, 0xFF, 0xFF));
        delay(30);
        send(id, rgb_color(0, 0, 0xFF), rgb_color(0, 0xFF, 0xFF));
        delay(30);
        send(id, rgb_color(0, 0, 0x00), rgb_color(0, 0xFF, 0xFF));
        delay(30);
        send(id, rgb_color(0, 0, 0xFF), rgb_color(0, 0xFF, 0xFF));
    }

public:
    static void debug(int id)
    {
        flicker(id);
        delay(500);
    }
};