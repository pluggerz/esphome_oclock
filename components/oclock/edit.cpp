#include "oclock.h"

#ifdef MASTER_MODE

#include "master.h"
#include "requests.h"

#define H_STEP 30

void fill_leds(oclock::RgbColorLeds &leds, const oclock::RgbColor &color)
{
  for (int idx = 0; idx < LED_COUNT; ++idx)
  {
    leds[idx] = color;
  }
}

class LedLayer
{
protected:
  virtual void start()
  {
  }

  virtual bool update(Millis t)
  {
    return false;
  }
  virtual void combine(oclock::RgbColorLeds &leds) const = 0;

public:
  virtual ~LedLayer() {}

  void forced_combine(oclock::RgbColorLeds &leds)
  {
    start();
    update(millis());
    combine(leds);
  }
};

class AbstractSettigsLayer : public LedLayer
{
  Millis lastTime{0};

  virtual void start() final override { lastTime = 0; }

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
  virtual ~AbstractSettigsLayer() {}
};

class ColorPickerSettingsLayer : public LedLayer
{

  static int scale_to_brightness(int scaled_brightness_) { return (1 << scaled_brightness_) - 1; }

  virtual void combine(oclock::RgbColorLeds &leds) const override
  {

    const auto h = oclock::master.get_background_color_h();

    leds[3] = oclock::RgbColor(0, 0, 0);

    leds[4] = oclock::RgbColor::h_to_rgb(h - 4 * H_STEP);
    leds[5] = oclock::RgbColor::h_to_rgb(h - 3 * H_STEP);

    leds[6] = oclock::RgbColor::h_to_rgb(h - 2 * H_STEP);
    leds[7] = oclock::RgbColor::h_to_rgb(h - 1 * H_STEP);

    leds[8] = oclock::RgbColor::h_to_rgb(h);
    leds[9] = oclock::RgbColor::h_to_rgb(h);
    leds[10] = oclock::RgbColor::h_to_rgb(h);

    leds[11] = oclock::RgbColor::h_to_rgb(h + 1 * H_STEP);
    leds[0] = oclock::RgbColor::h_to_rgb(h + 2 * H_STEP);

    leds[1] = oclock::RgbColor::h_to_rgb(h + 3 * H_STEP);
    leds[2] = oclock::RgbColor::h_to_rgb(h + 4 * H_STEP);
  }

public:
  virtual ~ColorPickerSettingsLayer() {}
};

class BrightnessSettingsLayer : public LedLayer
{

  static int scale_to_brightness(int scaled_brightness_) { return (1 << scaled_brightness_) - 1; }

  virtual void combine(oclock::RgbColorLeds &leds) const override
  {
    for (int scaled_brightness = 1; scaled_brightness <= 5; scaled_brightness++)
    {
      if (scaled_brightness == oclock::master.get_brightness())
        leds[(scaled_brightness + 6) % LED_COUNT] = oclock::RgbColor(0x00, 0xFF, 0x00);
      else
        leds[(scaled_brightness + 6) % LED_COUNT] = oclock::RgbColor(0xFF, 0xFF, 0xFF);
    }
  }

public:
  virtual ~BrightnessSettingsLayer() {}
};

BrightnessSettingsLayer brightnessSettingsLayer;
ColorPickerSettingsLayer colorPickerSettingsLayer;

#include "master.h"

#include "main.esphome.h"

#include "requests.h"

using namespace oclock;

void publish(select::Select *select)
{
  if (!select->has_state())
    return;
  select->set(select->state);
}

class ShowSpeedAnimationRequest final : public oclock::requests::AnimationRequest
{
  const EditMode mode_;

public:
  virtual void finalize() override final
  {
    // just rotate for some time
    Instructions instructions;
    auto speed = oclock::master.get_base_speed();
    switch (mode_)
    {
    case EditMode::Speed:
    {
      auto lambda = [&](int handle_id)
      {
        instructions.add(handle_id, DeflatedCmdKey(CLOCKWISE | RELATIVE, 4 * 720, speed));
      };
      instructions.iterate_handle_ids(lambda);
    }
    break;

    default:
      break;
    }
    sendInstructions(instructions);
  }

public:
  ShowSpeedAnimationRequest(EditMode mode) : mode_(mode) {}
};

class TrackSpeedTask final : public AsyncDelay
{
  int current_base_speed{-1},
      current_turn_speed{-1},
      current_turn_steps{-1};
  const EditMode mode_;

  virtual void step() override
  {
    auto base_speed = oclock::master.get_base_speed();
    if (current_base_speed == base_speed && current_turn_speed == Instructions::turn_speed && current_turn_steps == Instructions::turn_steps)
      return;

    esphome::delay(50);
    current_base_speed = base_speed;
    current_turn_speed = Instructions::turn_speed;
    current_turn_steps = Instructions::turn_steps;
    oclock::queue(new oclock::requests::WaitUntilAnimationIsDoneRequest());
    oclock::queue(new ShowSpeedAnimationRequest(mode_));
  };

public:
  TrackSpeedTask(EditMode mode) : AsyncDelay(100), mode_(mode) {}
};

class TrackBrightnessRequest final : public AsyncDelay
{
  int current_brightness;

  void follow()
  {
    current_brightness = oclock::master.get_brightness();
    ESP_LOGI(TAG, "TrackBrightnessRequest: current_brightness=%d", current_brightness);
    oclock::RgbColorLeds leds;
    brightnessSettingsLayer.forced_combine(leds);
    oclock::requests::publish_foreground_rgb_leds(leds);
  }

  virtual void step() override
  {
    if (current_brightness == oclock::master.get_brightness())
      return;
    follow();
  };

public:
  TrackBrightnessRequest() : AsyncDelay(30) { follow(); }
};

class TrackColorRequest final : public AsyncDelay
{
  int current_h = -1;

  void follow()
  {
    current_h = oclock::master.get_background_color_h();
    ESP_LOGI(TAG, "TrackColorRequest: current_h=%d", current_h);
    oclock::RgbColorLeds leds;
    colorPickerSettingsLayer.forced_combine(leds);
    oclock::requests::publish_foreground_rgb_leds(leds);
  }

  virtual void step() override
  {
    if (current_h == oclock::master.get_background_color_h())
      return;
    follow();
  };

public:
  TrackColorRequest() : AsyncDelay(30) { follow(); }
};

void publish_settings()
{
  if (!master.is_in_edit())
    return;

  AsyncRegister::byName("time_tracker", nullptr);
  const auto &mode = master.get_edit_mode();

  oclock::queue(new oclock::requests::EnterSettingsMode(mode));
  switch (mode)
  {
  case EditMode::BackgroundColor:
  {
    oclock::queue(new requests::ForegroundModeSelectRequest(ForegroundEnum::RgbColors));
    AsyncRegister::byName("time_tracker", new TrackColorRequest());

    ESP_LOGI(TAG, "Publishing edit mode: mode=%d/BackgroundColor", mode);
  }
  break;

  case EditMode::Brightness:
  {
    oclock::queue(new requests::ForegroundModeSelectRequest(ForegroundEnum::RgbColors));
    AsyncRegister::byName("time_tracker", new TrackBrightnessRequest());
    // nothing to do, just make sure we use the original settings

    ESP_LOGI(TAG, "Publishing edit mode: mode=%d/Brightness", mode);
    publish(esp_components.background);
  }
  break;

  case EditMode::Speed:
  {
    oclock::queue(new requests::ForegroundModeSelectRequest(ForegroundEnum::FollowHandles));

    ESP_LOGI(TAG, "Publishing edit mode: mode=%d/Speed or TurnSpeed or TurnSteps", mode);
    // speed
    AsyncRegister::byName("time_tracker", new TrackSpeedTask(mode));
  }
  break;

  default:
    // nothing to do, just make sure we use the original settings
    publish(esp_components.foreground);
    publish(esp_components.background);

    ESP_LOGI(TAG, "Publishing edit mode: mode=%d/Unknown", mode);
    break;
  }
};

void Master::edit_toggle()
{
  const auto &value = master.is_in_edit();
  if (value)
  {
    AsyncRegister::byName("time_tracker", nullptr);
    oclock::queue(new oclock::requests::EnterSettingsMode(oclock::EditMode::None));

    ESP_LOGI(TAG, "Leaving edit mode");
    publish(esp_components.foreground);
    publish(esp_components.background);
    publish(esp_components.active_mode);
  }
  else
  {
    const auto &mode = EditMode::Brightness;
    oclock::queue(new oclock::requests::EnterSettingsMode(mode));

    ESP_LOGI(TAG, "Changing to edit mode: mode = %d ->: %d", mode, master.is_in_edit());
    publish_settings();
  }
}

void oclock::Master::edit_next()
{
  if (!master.is_in_edit())
    return;

  auto mode = master.get_edit_mode();
  switch (mode)
  {
  case EditMode::Background:
    mode = master.get_led_background_mode() == BackgroundEnum::SolidColor
               ? EditMode::BackgroundColor
               : EditMode::Speed;
    break;

  default:
    const int raw_mode = int(mode) + 1;
    mode = raw_mode > int(EditMode::Last)
               ? EditMode::First
               : EditMode(raw_mode);
    break;
  }
  master.set_edit_mode(mode);
  ESP_LOGI(TAG, "Changing to edit mode: mode = %d", mode);
  publish_settings();
}

float calc_next(int step, float value)
{
  value = (value * 31.0 + step) / 31.0;
  if (value < 0.0)
    return 0.0;
  if (value > 1.0)
    return 1.0;
  return value;
}

void publish_next(select::Select *select)
{
  const auto options = select->traits.get_options();
  if (!options.size())
    return;
  if (!select->has_state())
  {
    select->set(options.front());
    return;
  }
  const auto &state = select->state;
  for (auto it = options.begin(); it != options.end(); it++)
  {
    if (*it == state)
    {
      ++it;
      select->set(it == options.end() ? options.front() : *it);
      return;
    }
  }
}

void publish_previous(select::Select *select)
{
  const auto options = select->traits.get_options();
  if (!options.size())
    return;
  if (!select->has_state())
  {
    select->set(options.back());
    return;
  }
  const auto &state = select->state;
  for (auto it = options.rbegin(); it != options.rend(); it++)
  {
    if (*it == state)
    {
      ++it;
      select->set(it == options.rend() ? options.back() : *it);
      return;
    }
  }
}

void edit_add_value(int direction, bool big)
{
  if (!master.is_in_edit())
    return;

  const auto &mode = master.get_edit_mode();
  switch (mode)
  {
  case EditMode::Brightness:
  {
    // update the light via the component:
    auto component = oclock::esp_components.brightness;
    auto brightness = component->state + direction;
    component->set(brightness);
  }
  break;

  case EditMode::Background:
  {
    auto component = oclock::esp_components.background;
    ESP_LOGI(TAG, "EditMode::Background before -> %s", component->state.c_str());
    if (direction > 0)
    {
      publish_next(component);
    }
    else
    {
      publish_previous(component);
    }
    ESP_LOGI(TAG, "EditMode::Background after -> %s", component->state.c_str());
  }
  break;

  case EditMode::BackgroundColor:
  {
    int old_h = oclock::master.get_background_color_h();
    int new_h = (old_h + direction * H_STEP) % 360;

    auto color = oclock::RgbColor::h_to_rgb(new_h);
    ESP_LOGI(TAG, "EditMode::BackgroundColor after h=%d -> %d (real: %d)", old_h, new_h, color.as_h());

    auto call = oclock::esp_components.light->make_call();
    call.set_transition_length(1);
    call.set_red(color.get_red() / 255.);
    call.set_green(color.get_green() / 255.);
    call.set_blue(color.get_blue() / 255.);
    call.perform();
  }
  break;

  case EditMode::Speed:
  {

    float current = oclock::esp_components.speed->state;
    float next = current + direction * (big ? 4 : 1);
    oclock::esp_components.speed->set(next);
    ESP_LOGI(TAG, "EditMode::Speed %f -> %f", current, next);
  }
  break;

  default:
    ESP_LOGI(TAG, "increment/decrement for mode %d not supported !?", mode);
    break;
  }
}

void oclock::Master::edit_plus(bool big)
{
  edit_add_value(1, big);
}

void oclock::Master::edit_minus(bool big)
{
  edit_add_value(-1, big);
}
#endif