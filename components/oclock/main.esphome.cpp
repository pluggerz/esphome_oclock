#include "oclock.h"

#ifdef MASTER_MODE
#include "main.esphome.h"

using namespace oclock;

oclock::EspComponents oclock::esp_components;

template <class V>
class AbstractMap
{
public:
    typedef std::map<std::string, V> Modes;

protected:
    Modes modes;

public:
    optional<V> find(const std::string &value) const
    {
        auto ret = modes.find(value);
        if (ret == modes.end())
            return {};
        return ret->second;
    }
};

class TrackTimeTask final : public AsyncDelay
{
    const oclock::time_tracker::TimeTracker &tracker;
    int last_minute{tracker.now().minute};

    virtual void setup(Micros _) override
    {
        AsyncDelay::setup(_);

        auto now = tracker.now();
        ESP_LOGI(TAG, "Now is the time %02d:%02d (%S) note: last_minute=%d", now.hour, now.minute, tracker.name(), last_minute);
    }

    virtual void step() override
    {
        auto now = tracker.now();
        int minute = now.minute;
        if (minute == last_minute)
        {
            return;
        }
        last_minute = minute;
        ESP_LOGI(TAG, "Lets animate for time %02d:%02d (%S) note: last_minute=%d", now.hour, now.minute, tracker.name(), last_minute);
        oclock::queue(new requests::TrackTimeRequest(tracker));
    };

public:
    TrackTimeTask(const oclock::time_tracker::TimeTracker &tracker) : AsyncDelay(100), tracker(tracker) {}
};

class ActiveModeMap : public AbstractMap<ActiveMode>
{
public:
    ActiveModeMap()
    {
        modes["None"] = ActiveMode::None;
        modes["TrackHassioTime"] = ActiveMode::TrackHassioTime;
        modes["TrackInternalTime"] = ActiveMode::TrackInternalTime;
        modes["TrackTestTime"] = ActiveMode::TrackTestTime;
    }
} active_mode_map;

void active_mode_callback(const std::string &mode)
{
    auto value = active_mode_map.find(mode);
    if (!value)
        return;

    oclock::master.set_active_mode(*value);
    switch (*value)
    {
    case ActiveMode::TrackHassioTime:
        AsyncRegister::byName("time_tracker", new TrackTimeTask(oclock::time_tracker::realTimeTracker));
        break;

    case ActiveMode::TrackInternalTime:
        AsyncRegister::byName("time_tracker", new TrackTimeTask(oclock::time_tracker::internalClockTimeTracker));
        break;

    case ActiveMode::TrackTestTime:
        AsyncRegister::byName("time_tracker", nullptr);
        class TrackTestTime : public Async
        {
            oclock::time_tracker::Text text;
            Millis t0 = ::millis();

        public:
            virtual void loop(Micros) override
            {
                if (::millis() - t0 < 2000)
                    return;
                t0 = ::millis();

                auto new_text = oclock::time_tracker::testTimeTracker.to_text();
                if (text.__equal__(new_text))
                    return;
                text = new_text;
                oclock::queue(new requests::TrackTimeRequest(oclock::time_tracker::testTimeTracker));
            }
        };
        AsyncRegister::byName("time_tracker", new TrackTestTime());
        break;

    case ActiveMode::None:
        AsyncRegister::byName("time_tracker", nullptr);
        break;
    }
}

class BackgroundLedMap : public AbstractMap<BackgroundEnum>
{
public:
    BackgroundLedMap()
    {
        modes["Solid Color"] = BackgroundEnum::SolidColor;
        modes["Warm White Shimmer"] = BackgroundEnum::WarmWhiteShimmer;
        modes["Random Color Walk"] = BackgroundEnum::RandomColorWalk;
        modes["Traditional Colors"] = BackgroundEnum::TraditionalColors;
        modes["Color Explosion"] = BackgroundEnum::ColorExplosion;
        modes["Gradient"] = BackgroundEnum::Gradient;
        modes["Bright Twinkle"] = BackgroundEnum::BrightTwinkle;
        modes["Collision"] = BackgroundEnum::Collision;
        modes["Rainbow"] = BackgroundEnum::Rainbow;
        modes["Rgb Colors"] = BackgroundEnum::RgbColors;
    }
} background_led_map;

void background_led_callback(const std::string &mode)
{
    auto value = background_led_map.find(mode);
    if (value)
    {
        oclock::queue(new requests::BackgroundModeSelectRequest(*value));
        ESP_LOGI(__FUNCTION__, "set to %s", mode.c_str());
    }
    else
        ESP_LOGE(__FUNCTION__, "NOT FOUND: %s", mode.c_str());
}

class ForegroundLedMap : public AbstractMap<ForegroundEnum>
{
public:
    ForegroundLedMap()
    {
        modes["None"] = ForegroundEnum::None;
        modes["Debug Leds"] = ForegroundEnum::DebugLeds;
        modes["Follow Handles"] = ForegroundEnum::FollowHandles;
    }
} foreground_led_map;

void foreground_led_callback(const std::string &mode)
{
    auto value = foreground_led_map.find(mode);
    if (value)
    {
        oclock::queue(new requests::ForegroundModeSelectRequest(*value));
        ESP_LOGI(__FUNCTION__, "set to %s", mode.c_str());
    }
    else
        ESP_LOGE(__FUNCTION__, "NOT FOUND: %s", mode.c_str());
}

class HandlesAnimationMap : public AbstractMap<HandlesAnimationEnum>
{
public:
    HandlesAnimationMap()
    {
        modes["Random"] = HandlesAnimationEnum::Random;
        modes["Swipe"] = HandlesAnimationEnum::Swipe;
        modes["Distance"] = HandlesAnimationEnum::Distance;
    }
} handles_animation_map;

void handles_animation_callback(const std::string &mode)
{
    auto value = handles_animation_map.find(mode);
    if (value)
    {
        oclock::master.set_handles_animation_mode(*value);
        ESP_LOGI(__FUNCTION__, "set to %s", mode.c_str());
    }
    else
        ESP_LOGE(__FUNCTION__, "NOT FOUND: %s", mode.c_str());
};

class HandleDistanceMap : public AbstractMap<HandlesDistanceEnum>
{
public:
    HandleDistanceMap()
    {
        modes["Random"] = HandlesDistanceEnum::Random;
        modes["Left"] = HandlesDistanceEnum::Left;
        modes["Right"] = HandlesDistanceEnum::Right;
        modes["Shortest"] = HandlesDistanceEnum::Shortest;
    }
} handle_distance_map;

void distance_calculator_callback(const std::string &mode)
{
    auto value = handle_distance_map.find(mode);
    if (value)
    {
        oclock::master.set_handles_distance_mode(*value);
        ESP_LOGI(__FUNCTION__, "set to %s", mode.c_str());
    }
    else
        ESP_LOGE(__FUNCTION__, "NOT FOUND: %s", mode.c_str());
};

class HandlesInBetweenAnimationModeMap : public AbstractMap<InBetweenAnimationEnum>
{
public:
    HandlesInBetweenAnimationModeMap()
    {
        modes["Random"] = InBetweenAnimationEnum::Random;
        modes["None"] = InBetweenAnimationEnum::None;
        modes["Star"] = InBetweenAnimationEnum::Star;
        modes["Dash"] = InBetweenAnimationEnum::Dash;
        modes["Middle Point 1"] = InBetweenAnimationEnum::Middle1;
        modes["Middle Point 2"] = InBetweenAnimationEnum::Middle1;
        modes["Pac Man"] = InBetweenAnimationEnum::PacMan;
    }
} handles_in_between_animation_mode_map;

void handles_in_between_animation_mode_callback(const std::string &mode)
{
    auto value = handles_in_between_animation_mode_map.find(mode);
    if (value)
    {
        oclock::master.set_in_between_animation_mode(*value);
        ESP_LOGI(__FUNCTION__, "set to %s", mode.c_str());
    }
    else

        ESP_LOGE(__FUNCTION__, "NOT FOUND: %s", mode.c_str());
};

void speed_callback(float value)
{
    oclock::master.set_base_speed(value);

    ESP_LOGI(__FUNCTION__, "set to %f", value);
};

void turn_speed_callback(float value)
{
    Instructions::turn_speed = value;
    ESP_LOGI(__FUNCTION__, "set to %f", value);
};

void brightness_callback(float value)
{
    oclock::requests::publish_brightness(value);
}

void text_callback(const std::string &value)
{
    auto it = value.begin();
    const auto end = value.end();
    oclock::time_tracker::Text text;
    text.ch0 = it == end ? '*' : *(it++);
    text.ch1 = it == end ? '*' : *(it++);
    text.ch2 = it == end ? '*' : *(it++);
    text.ch3 = it == end ? '*' : *(it++);

    ESP_LOGI(__FUNCTION__, "set to %s -> ['%c', '%c', '%c', '%c']", value.c_str(), text.ch0, text.ch1, text.ch2, text.ch3);
    oclock::time_tracker::testTimeTracker.set(text);
}

#define init(number, callback)                  \
    number->add_on_state_callback(callback);    \
    if (number->has_state())                    \
    {                                           \
        callback(number->state);                \
        esphome::delay(1); /* FIX CRC ERRORS */ \
    }

#define init_number(number, callback) \
    init(number, callback)

#define init_select(select, callback) \
    init(select, callback)

void update_from_components()
{
    RgbColorLeds leds;
    for (int idx = 0; idx < LED_COUNT; ++idx)
        leds[idx] = oclock::RgbColor::HtoRGB(idx * 30);
    oclock::requests::publish_rgb_leds(leds);

    // handles  animation
    init_select(oclock::esp_components.handles_animation, handles_animation_callback);
    // distance calculculator
    init_select(oclock::esp_components.distance_calculator, distance_calculator_callback);
    // inbetween animation
    init_select(oclock::esp_components.inbetween_animation, handles_in_between_animation_mode_callback);
    // background mode
    init_select(oclock::esp_components.background, background_led_callback);
    // foreground mode
    init_select(oclock::esp_components.foreground, foreground_led_callback);
    // brightness
    init_number(oclock::esp_components.brightness, brightness_callback);
    // speed
    init_number(oclock::esp_components.speed, speed_callback);
    // text
    init(oclock::esp_components.text, text_callback);
    // acctive mode (do finally)
    init_select(oclock::esp_components.active_mode, active_mode_callback);
};

#endif