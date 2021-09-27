#pragma once

namespace oclock
{

    class Master
    {
        long op_delay_t_millis{0};
        void loop_pre_accept();

    public:
        void setup();
        void loop();
        void dump_config();
    };

    extern Master master;
} // namespace oclock