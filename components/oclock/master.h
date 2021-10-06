#pragma once

#include "esphome/components/time/real_time_clock.h"

namespace oclock
{
    typedef std::function<void(Millis)> LoopFunc;
   

    class Master
    {
        esphome::time::RealTimeClock *time_ = nullptr;
        LoopFunc loopFunc_{};

        long op_delay_t_millis{0};
        void loop_pre_accept(Millis t);

    public:
        // all steps before the master is ready
        void change_to_init();
        void change_to_accepting();
        void change_to_accepted();

    public:
        void set_time(esphome::time::RealTimeClock *time) { time_ = time; }
        void setup();
        void loop();
        void dump_config();
    };

    extern Master master;
} // namespace oclock