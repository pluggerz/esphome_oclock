#pragma once

#include <FastGPIO.h>
#define APA102_USE_FAST_GPIO

#include <APA102.h>

#include "pins.h"
#include "hal.h"

#include "slave.h"

// Create an object for writing to the LED strip.
extern APA102<LED_DATA_PIN, LED_CLOCK_PIN> ledStrip;

// Set the number of LEDs to control.
constexpr int LED_COUNT = 12;

typedef struct rgba_color
{
    uint8_t red, green, blue;
    uint8_t alpha = 31;

    void reset()
    {
        red = green = blue = 0;
        alpha = 31;
    }

    void off()
    {
        red = green = blue = 0;
        alpha = 0;
    }
    operator rgb_color() const { return rgb_color(red, green, blue); }
    rgba_color(){};
    rgba_color(const rgb_color &c, uint8_t a) : red(c.red), green(c.green), blue(c.blue), alpha(a){};
    rgba_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 31) : red(r), green(g), blue(b), alpha(a){};
} rgba_color;

typedef rgba_color Leds[LED_COUNT];

class LedLayer
{
    friend class LedAnimation;

protected:
    typedef unsigned long Offset;

public:
    virtual void start()
    {
    }

    virtual bool update(Millis t)
    {
        return false;
    }
    virtual void combine(Leds &leds) const = 0;

    virtual ~LedLayer() {}
};

class BackgroundLayer : public LedLayer
{
protected:
    int brightness = 31;
};

class ForegroundLayer : public LedLayer
{
};

class LedAsync
{
private:
    BackgroundLayer *backgroundLayer_{nullptr};
    ForegroundLayer *foregroundLayer_{nullptr};
    // int scaled_brightness_ = 5;
    Millis last{0};
    Leds leds;

    void updateForground()
    {
    }

    bool updateFrame(Leds &leds)
    {
        Hal::yield();
        ledStrip.startFrame();
        Hal::yield();
        const auto brightness=get_brightness();
        for (uint8_t i = 1; i <= LED_COUNT; i++)
        {
            const auto &led = leds[i % LED_COUNT];
            int8_t forced_alpha=led.alpha;
            int alpha;
            if (forced_alpha < 0)
                alpha=-forced_alpha;
            else
                alpha = round((double)led.alpha * (double)brightness / 31.0);
            if (alpha > 31)
            {
                alpha = 31;
            }
            ledStrip.sendColor(led, alpha);
            Hal::yield();
        }
        ledStrip.endFrame(LED_COUNT);
        Hal::yield();
        return true;
    }
public:
    void updateLeds()
    {
        if (backgroundLayer_)
        {
            backgroundLayer_->combine(leds);
        }
        else
        {
            auto off = rgba_color(0xFF, 0xFF, 0xFF);
            for (uint8_t i = 0; i < LED_COUNT; i++)
                leds[i] = off;
        }
        if (foregroundLayer_)
        {
            foregroundLayer_->combine(leds);
        }
        updateFrame(leds);
    }

public:
    void loop(Micros nowMicros)
    {
        auto now = nowMicros / 1000;

        if (now - last < 20)
        {
            return;
        }
        last = now;

        // background changed?
        if (backgroundLayer_)
        {
            if (backgroundLayer_->update(now))
            {
                if (foregroundLayer_)
                    foregroundLayer_->update(now);
                updateLeds();
                return;
            }
        }
        // foreground maybe?
        if (foregroundLayer_)
        {
            if (foregroundLayer_->update(now))
            {
                updateLeds();
                return;
            }
        }
    }

public:
    static int scale_to_brightness(int scaled_brightness_) { return (1 << scaled_brightness_)  -1; }

    int get_scaled_brightness() const { return slave_settings.get_scaled_brightness(); }
    int get_brightness() const { return scale_to_brightness(get_scaled_brightness()); }
    

    void set_led_layer(BackgroundLayer *ledLayer)
    {
        if (backgroundLayer_ == ledLayer)
            return;

        backgroundLayer_ = ledLayer;
        if (backgroundLayer_)
        {
            backgroundLayer_->start();
            backgroundLayer_->update(micros());
        }
        updateLeds();
    }

    void set_foreground_led_layer(ForegroundLayer *ledLayer)
    {
        if (foregroundLayer_ == ledLayer)
            return;

        foregroundLayer_ = ledLayer;
        if (foregroundLayer_)
        {
            foregroundLayer_->start();
            foregroundLayer_->update(micros());
        }
        updateLeds();
    }

    ForegroundLayer* get_foreground_led_layer() const {
        return foregroundLayer_;
    }

} extern ledAsync;

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
