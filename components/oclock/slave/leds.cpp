#include "leds.h"
#include "stepper.h"

// Create an object for writing to the LED strip.
APA102<LED_DATA_PIN, LED_CLOCK_PIN> ledStrip;

class DebugLayer : public ForegroundLayer
{
    Leds leds;
    Millis lastTime{0};

    // make 16..32 visible
    template <class S>
    int scaleSpeed(const S &s, int delay) const
    {
        auto steps_per_minute = s.calculate_speed_in_revs_per_minute(delay);
        auto ret = int((steps_per_minute - 12.0) / 4.0);

        if (ret < 0)
            return 0;
        if (ret > 6)
            return 6;
        return ret;
    }

    template <class S>
    void draw(Leds &leds, const S &s, int offset) const
    {
        auto red = rgba_color(0xFF, 0x00, 0x00);
        auto green = rgba_color(0x00, 0xFF, 0x00);
        int expectedLeds = scaleSpeed(s, s.step_delay);
        int currentLeds = scaleSpeed(s, s.step_current);
        for (int idx = 0; idx < currentLeds; ++idx)
        {
            leds[offset + idx] = idx <= expectedLeds ? green : red;
        }
        if (s.defecting)
        {
            leds[offset + 6].blue = 0xFF;
        }
    }

    virtual void combine(Leds &leds) const override
    {
        auto blue = rgba_color(0x00, 0x00, 0x00);
        for (int idx = 0; idx < LED_COUNT; ++idx)
        {
            leds[idx] = blue;
        }
        draw(leds, stepper0, 0);
        draw(leds, stepper1, 6);
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

ForegroundLayer &debugLedLayer()
{
    return debugLedLayer_;
}