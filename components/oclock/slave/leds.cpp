#include "leds.h"
#include "stepper.h"

#include "leds_foreground.h"

// Create an object for writing to the LED strip.
APA102<LED_DATA_PIN, LED_CLOCK_PIN> ledStrip;

class DebugLayer : public ForegroundLayer
{
    Leds leds;
    Millis lastTime{0};

    static inline void combine_channel(uint8_t &current, const uint8_t request) __attribute__((always_inline))
    {
        const auto weight = .7f;
        current = current * (1 - weight) + request * weight;
    }
    static inline void combine(rgba_color &current, const rgba_color &request) __attribute__((always_inline))
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
        draw(leds, stepper0, 0, 10);
        draw(leds, stepper1, 6, 3);
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
FollowHandlesLedLayer followHandlesLayer_;

ForegroundLayer &debugLedLayer()
{
    return debugLedLayer_;
}

ForegroundLayer &followHandlesLayer()   {
    return followHandlesLayer_;
}