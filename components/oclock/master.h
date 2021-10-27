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


    class Master
    {
        SlaveConfig slaves_[24];
        int background_led_mode_{0};
        int brightness_{0};

        long op_delay_t_millis{0};
        void loop_pre_accept(Millis t);

    public:
        void reset();
        SlaveConfig &slave(int idx) { return slaves_[idx]; }

        int get_led_background_mode() const { return background_led_mode_; }
        void set_led_background_mode(int value) { background_led_mode_=value; }

        int get_brightness() const { return brightness_; }
        void set_brightness(int value) { brightness_=value; }
        

    public:
        void setup();
        void loop();
        void dump_config();
    };
    
    extern Master master;
} // namespace oclock