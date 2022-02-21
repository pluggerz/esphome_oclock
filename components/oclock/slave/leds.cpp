#include "leds.h"
#include "stepper.h"
#include "slave.h"
#include "leds_foreground.h"

// Create an object for writing to the LED strip.
APA102<LED_DATA_PIN, LED_CLOCK_PIN> ledStrip;

uint8_t HighlightLedLayer::ledAlphas[LED_COUNT] = {};

class SettingsLayer : public ForegroundLayer
{
private:
    int8_t mode_{-1};
    Millis lastTime{0};

    void combine_brightness(Leds &leds) const
    {
        int active_scaled_brightness = ledAsync.get_scaled_brightness();
        for (int scaled_brightness = 1; scaled_brightness <= 5; scaled_brightness++)
        {
            int8_t brightness = ledAsync.scale_to_brightness(scaled_brightness);
            if (scaled_brightness == active_scaled_brightness)
                leds[scaled_brightness + 1] = rgba_color(0x00, 0xFF, 0x00, -brightness);
            else
                leds[scaled_brightness + 1] = rgba_color(0xFF, 0xFF, 0xFF, -brightness);
        }
    }

    void combine_background(Leds &leds) const {
        
     };

    virtual void combine(Leds &leds) const override
    {
        switch (mode_)
        {
        case 0:
            combine_brightness(leds);
            return;

        case 1:
            combine_background(leds);
            return;

        default:
            return;
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
    void set_mode(int8_t mode)
    {
        this->mode_ = mode;
    }
};

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

DebugLayer debugLedLayer_;
SettingsLayer settingsLedLayer_;
FollowHandlesLedLayer followHandlesLayer_;

ForegroundLayer &debugLedLayer()
{
    return debugLedLayer_;
}

ForegroundLayer &settingsLedLayer(int8_t mode)
{
    settingsLedLayer_.set_mode(mode);
    return settingsLedLayer_;
}

ForegroundLayer &followHandlesLayer(bool forced)
{
    followHandlesLayer_.forced = forced;
    return followHandlesLayer_;
}