#pragma once

#include "enums.h"

typedef unsigned long Millis;

namespace oclock
{
    enum class EditMode
    {
        None = -1,
        First = 0,
        Brightness = First,
        Background,
        BackgroundColor,
        Speed,
        Last = Speed
    };

    class SlaveConfig
    {
    public:
        int animator_id{-1};
        class Handle
        {
        public:
            int magnet_offset{0};
            int initial_ticks{0};
        };
        Handle handles[2];
    };

    class Master
    {
        uint32_t baud_rate = 9600;
        SlaveConfig slaves_[24];
        BackgroundEnum background_led_mode_{BackgroundEnum::First};
        ForegroundEnum foreground_led_mode_{ForegroundEnum::First};
        int background_color_h_ = {0};
        int brightness_{0};
        int base_speed{12};
        ActiveMode active_mode{ActiveMode::None};

        bool calibrating{false};
        bool in_edit{false};
        EditMode edit_mode{EditMode::Brightness};

        long op_delay_t_millis{0};
        void loop_pre_accept(Millis t);
        InBetweenAnimationEnum in_between_mode = InBetweenAnimationEnum::Random;
        HandlesAnimationEnum handles_mode = HandlesAnimationEnum::Random;
        HandlesDistanceEnum distance_mode = HandlesDistanceEnum::Random;

    public:
        void reset();
        SlaveConfig &slave(int idx) { return slaves_[idx]; }

        ActiveMode get_active_mode() const { return active_mode; }
        void set_active_mode(const ActiveMode &value) { active_mode = value; }

        int get_background_color_h() const { return background_color_h_; }
        void set_background_color_h(int value) { background_color_h_ = value; }

        void set_baud_rate(uint32_t value) { baud_rate = value; }
        uint32_t get_baud_rate() const { return baud_rate; }

        int get_base_speed() const { return base_speed; }
        void set_base_speed(int value) { base_speed = value; }

        ForegroundEnum get_led_foreground_mode() const { return foreground_led_mode_; }
        void set_led_foreground_mode(ForegroundEnum value) { foreground_led_mode_ = value; }

        BackgroundEnum get_led_background_mode() const { return background_led_mode_; }
        void set_led_background_mode(BackgroundEnum value) { background_led_mode_ = value; }

        int get_brightness() const { return brightness_; }
        void set_brightness(int value) { brightness_ = value; }

        void set_handles_distance_mode(HandlesDistanceEnum value) { distance_mode = value; }
        HandlesDistanceEnum get_handles_distance_mode() const { return distance_mode; }

        void set_handles_animation_mode(HandlesAnimationEnum value) { handles_mode = value; }
        HandlesAnimationEnum get_handles_animation_mode() const { return handles_mode; }

        void set_in_between_animation_mode(InBetweenAnimationEnum value) { in_between_mode = value; }
        InBetweenAnimationEnum get_in_between_animation() const { return in_between_mode; }

        void set_edit_mode(EditMode value) { edit_mode = value; }
        EditMode get_edit_mode() const { return edit_mode; }

        void set_in_edit(bool value) { in_edit = value; }
        bool is_in_edit() const { return in_edit; }

        // edit interactions
        void calibrate_toggle();
        void edit_toggle();
        void edit_next();
        void edit_plus(bool big = false);
        void edit_minus(bool big = false);

    public:
        void setup();
        void loop();
        void dump_config();
    };

    extern Master master;
} // namespace oclock