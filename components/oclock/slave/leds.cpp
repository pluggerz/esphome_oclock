#include "leds.h"
#include "stepper.h"
#include "slave.h"
#include "leds_foreground.h"

// Create an object for writing to the LED strip.
APA102<LED_DATA_PIN, LED_CLOCK_PIN> ledStrip;

uint8_t HighlightLedLayer::ledAlphas[LED_COUNT] = {};

class DebugLayer : public ForegroundLayer
{
    Leds leds;
    Millis lastTime{0};

    static void combine_channel(uint8_t &current, const uint8_t request)
    {
        const auto weight = .7f;
        current = current * (1 - weight) + request * weight;
    }
    static void combine(rgba_color &current, const rgba_color &request)
    {
        combine_channel(current.red, request.red);
        combine_channel(current.green, request.green);
        combine_channel(current.blue, request.blue);
        combine_channel(current.alpha, request.alpha);
    }

private:
    template <class S>
    void draw(Leds &leds, const S &s, int offset, int magnet_led) const
    {
        // behind or not
        if (s.last_step_time == 0 || !s.is_behind())
        {
            combine(leds[offset + 0], rgba_color(0x00, 128, 10));
        }
        else
        {
            combine(leds[offset + 0], rgba_color(0xFF, 128, 0));
        }
        // speed indication
        if (s.step_current > s.step_delay)
        {
            combine(leds[offset + 1], rgba_color(30, 0xFF, 0, 16));
        }
        else
        {
            auto diff = (s.step_delay - s.step_on_fail_delay) / 3;
            if (s.step_current > s.step_delay - diff)
            {
                combine(leds[offset + 1], rgba_color(0, 0xFF, 0, 31));
            }
            else if (s.step_current > s.step_delay - 2 * diff)
            {
                combine(leds[offset + 1], rgba_color(0xFF, 0xFF, 0, 31));
            }
            else
            {
                combine(leds[offset + 1], rgba_color(0xFF, 0x00, 0, 31));
            }
        }

        // defecting (turning for exampel)
        if (s.defecting)
        {
            combine(leds[offset + 2], rgba_color(0, 0, 0xFF));
        }
        // if we see the magnet
        if (s.get_magnet_pin())
        {
            combine(leds[magnet_led], rgba_color(0xFF, 0xFF, 0xFF));
        }
    }

    virtual void combine(Leds &leds) const override
    {
        // draw(leds, stepper0, 0, 10);
        // draw(leds, stepper1, 6, 3);
        draw(leds, stepper0, 0, 6);
        draw(leds, stepper1, 6, 6);
    }

    virtual bool update(Millis now) override final
    {
        if (now - lastTime > 100)
        {
            lastTime = now;
            return true;
        }
        return false;
    }
};

class RgbLedLayer : public BackgroundLayer, public ForegroundLayer
{
    oclock::RgbColorLeds leds_;
    // Leds leds_;
    Millis lastTime{0};

    virtual void combine(Leds &leds) const override
    {
        for (int idx = 0; idx < LED_COUNT; ++idx)
        {
            const auto &led = leds_[idx];
            if (led.invisible())
                continue;

            leds[idx] = led;
        }
    }

    virtual bool update(Millis now) override final
    {
        if (now - lastTime > 100)
        {
            lastTime = now;
            return true;
        }
        return false;
    }

public:
    void set(const oclock::RgbColorLeds &leds)
    {
        for (int idx = 0; idx < LED_COUNT; ++idx)
        {
            leds_[idx] = leds[idx];
        }
    };

    void set(const oclock::RgbColor &color)
    {
        // leds_ = leds;
        for (int idx = 0; idx < LED_COUNT; ++idx)
        {
            leds_[idx] = color;
        }
    };
};

RgbLedLayer rgbForegroundLedLayer_;
RgbLedLayer rgbBackgroundLedLayer_;

BackgroundLayer &rgbLedBackgroundLayer(const oclock::RgbColor &color)
{
    rgbBackgroundLedLayer_.set(color);
    return rgbBackgroundLedLayer_;
}

BackgroundLayer &rgbLedBackgroundLayer(const oclock::RgbColorLeds &leds)
{
    rgbBackgroundLedLayer_.set(leds);
    return rgbBackgroundLedLayer_;
}

BackgroundLayer &rgbLedBackgroundLayer()
{
    return rgbBackgroundLedLayer_;
}

ForegroundLayer &rgbLedForegroundLayer(const oclock::RgbColorLeds &leds)
{
    rgbForegroundLedLayer_.set(leds);
    return rgbForegroundLedLayer_;
}

ForegroundLayer &rgbLedForegroundLayer()
{
    return rgbForegroundLedLayer_;
}

DebugLayer debugLedLayer_;
FollowHandlesLedLayer followHandlesLayer_;

ForegroundLayer &debugLedLayer()
{
    return debugLedLayer_;
}

ForegroundLayer &followHandlesLayer(bool forced)
{
    followHandlesLayer_.forced = forced;
    return followHandlesLayer_;
}