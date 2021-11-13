#pragma once

namespace oclock
{
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

    enum class InBetweenAnimationEnum
    {
        Random,
        None,
        Star,
        Dash,
        Middle1,
        Middle2,
        PacMan,
    };
    
    enum class HandlesDistanceEnum 
    {
        Random,
        Shortest,
        Left,
        Right,
    };

    enum class HandlesAnimationEnum
    {
        Random,
        Swipe,
        Distance,
    };

    class Master
    {
        SlaveConfig slaves_[24];
        int background_led_mode_{0};
        int brightness_{0};

        long op_delay_t_millis{0};
        void loop_pre_accept(Millis t);
        InBetweenAnimationEnum in_between_mode = InBetweenAnimationEnum::Random;
        HandlesAnimationEnum handles_mode = HandlesAnimationEnum::Random;
        HandlesDistanceEnum distance_mode = HandlesDistanceEnum::Random;

    public:
        void reset();
        SlaveConfig &slave(int idx) { return slaves_[idx]; }

        int get_led_background_mode() const { return background_led_mode_; }
        void set_led_background_mode(int value) { background_led_mode_ = value; }

        int get_brightness() const { return brightness_; }
        void set_brightness(int value) { brightness_ = value; }

        void set_handles_distance_mode(HandlesDistanceEnum value) { distance_mode = value; }
        HandlesDistanceEnum get_handles_distance_mode() const { return distance_mode; }


        void set_handles_animation_mode(HandlesAnimationEnum value) { handles_mode = value; }
        HandlesAnimationEnum get_handles_animation_mode() const { return handles_mode; }

        void set_in_between_animation_mode(InBetweenAnimationEnum value) { in_between_mode = value; }
        InBetweenAnimationEnum get_in_between_animation() const { return in_between_mode; }

    public:
        void setup();
        void loop();
        void dump_config();
    };

    extern Master master;
} // namespace oclock