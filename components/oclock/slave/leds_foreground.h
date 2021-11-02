#include "leds.h"
#include "stepper.h"


ForegroundLayer& debugLedLayer();

class HighlightLedLayer : public ForegroundLayer
{
protected:
    const int MAX_BRIGHTNESS = 31;
    Millis lastTime{0};

    uint8_t ledAlphas[LED_COUNT] = {};

    inline int sparkle(int alpha) __attribute__((always_inline))
    {
        return alpha < 24 ? alpha * .6 : alpha;
    }

    virtual void combine(Leds &result) const override
    {
        double brightness = 0;

        for (int idx = 0; idx < LED_COUNT; ++idx)
        {
            brightness += result[idx].alpha;
        }
        brightness /= LED_COUNT;

        for (int idx = 0; idx < LED_COUNT; ++idx)
        {
            auto &r = result[idx];
            auto relativeBrightness = static_cast<double>(ledAlphas[idx]) / static_cast<double>(MAX_BRIGHTNESS) * brightness;
            // r.alpha = relativeBrightness;

            // Hal::yield();
        }
    }

    virtual bool update_layer(Millis now) = 0;

    virtual bool update(Millis now) override final
    {
        if (now - lastTime > 100)
        {
            lastTime = now;
            return update_layer(now);
        }
        return false;
    }
};

class FollowHandlesLedLayer : public HighlightLedLayer
{
    // to be able to keep up with the updates MAX_STAPS should not be too great
    const Offset MAX_STEPS = 12 * 4;

    /**
     * Convert to range [0..MAX_STEPS)  
     */
    inline Offset toHandleOffset(Millis now, int poshandlePos)
    {
        return MAX_STEPS * poshandlePos / NUMBER_OF_STEPS;
    }

    void fillLeds(Offset offset)
    {
        double position = (double)LED_COUNT * (double)offset / (double)MAX_STEPS;
        double weight = 1.0 - (position - floor(position));

        int firstLed = position;
        if (firstLed == LED_COUNT)
            firstLed = 0;
        int secondLed = (firstLed + 1) % LED_COUNT;

        int alpha = MAX_BRIGHTNESS * weight;
        ledAlphas[firstLed] |= sparkle(alpha);
        ledAlphas[secondLed] |= sparkle(MAX_BRIGHTNESS - alpha);
    }

    virtual bool update_layer(Millis now) override
    {
        auto longHandlePos = toHandleOffset(now, stepper0.ticks());
        auto shortHandlePos = toHandleOffset(now, stepper1.ticks());

        // clear all
        for (int idx = 0; idx < LED_COUNT; ++idx)
        {
            ledAlphas[idx] = 0;
        }
        //Hal::yield();

        fillLeds(longHandlePos);
        //Hal::yield();

        fillLeds(shortHandlePos);
        //Hal::yield();
        return true;
    }

public:
    void start() override
    {
        LedLayer::start();
    }
};