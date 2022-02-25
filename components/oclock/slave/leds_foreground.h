#include "leds.h"

#include "stepper.h"

#include "enums.h"

ForegroundLayer &debugLedLayer();

ForegroundLayer &rgbLedForegroundLayer(const oclock::RgbColorLeds &leds);
ForegroundLayer &rgbLedForegroundLayer();

/**
 * @brief
 *
 * @param forced =true will use brightness 31 otherwise based one the background
 * @return ForegroundLayer&
 */
ForegroundLayer &followHandlesLayer(bool forced);

class HighlightLedLayer : public ForegroundLayer
{
protected:
  // to be able to keep up with the updates MAX_STAPS should not be too great
  const Offset MAX_STEPS = 12 * 4;

  const int MAX_BRIGHTNESS = 31;
  Millis lastTime{0};

  static uint8_t ledAlphas[LED_COUNT];

  inline int sparkle(int alpha) __attribute__((always_inline))
  {
    return alpha < 24 ? alpha * .6 : alpha;
  }

  void clear_all_leds()
  {
    memset(ledAlphas, 0, LED_COUNT);
  }

  virtual void combine(Leds &result) const override
  {
    double brightness = 0;
    if (forced)
    {
      brightness = 31;
    }
    else
    {
      for (int idx = 0; idx < LED_COUNT; ++idx)
      {
        brightness += result[idx].alpha;
      }
      brightness /= LED_COUNT;
    }

    for (int idx = 0; idx < LED_COUNT; ++idx)
    {
      auto &r = result[idx];
      auto relativeBrightness = static_cast<double>(ledAlphas[idx]) / static_cast<double>(MAX_BRIGHTNESS) * brightness;
      r.alpha = relativeBrightness;
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

  void add_sparkle(Offset offset)
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

public:
  bool forced{false};
};

class FollowHandlesLedLayer : public HighlightLedLayer
{
  /**
   * Convert to range [0..MAX_STEPS)
   */
  inline Offset toHandleOffset(Millis now, int poshandlePos)
  {
    return MAX_STEPS * poshandlePos / NUMBER_OF_STEPS;
  }

  virtual bool update_layer(Millis now) override
  {
    auto longHandlePos = toHandleOffset(now, stepper0.ticks());
    auto shortHandlePos = toHandleOffset(now, stepper1.ticks());

    // clear all
    clear_all_leds();
    // fill leds
    add_sparkle(longHandlePos);
    add_sparkle(shortHandlePos);
    return true;
  }
};
