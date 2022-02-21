#include "oclock.h"

#ifdef MASTER_MODE

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

void publish_settings()
{
  AsyncRegister::byName("time_tracker", nullptr);
  const auto &mode = master.get_edit_mode();

  oclock::queue(new oclock::requests::EnterSettingsMode(mode));
  switch (mode)
  {
  case EditMode::Brightness:
    // nothing to do, just make sure we use the original settings
    ESP_LOGI(TAG, "Publishing edit mode: mode=%d/Brightness", mode);
    publish(esp_components.background);
    break;

  case EditMode::Speed:
  {
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
    ESP_LOGI(TAG, "Leaving edit mode");
    oclock::queue(new oclock::requests::EnterSettingsMode(oclock::EditMode::None));
    publish(esp_components.foreground);
    publish(esp_components.background);
    publish(esp_components.active_mode);
  }
  else
  {
    const auto &mode = EditMode::Brightness;
    master.set_edit_mode(mode);
    ESP_LOGI(TAG, "Changing to edit mode: mode = %d", mode);
    publish_settings();
  }
  master.set_in_edit(!value);
}

void oclock::Master::edit_next()
{
  if (!master.is_in_edit())
    return;

  auto raw_mode = int(master.get_edit_mode()) + 1;
  if (raw_mode > int(EditMode::Last))
    raw_mode = 0;
  const auto &mode = EditMode(raw_mode);
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