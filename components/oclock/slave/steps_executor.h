#pragma once

#include "interop.h"
#include "interop.keys.h"
#include "stepper.h"

class StepExecutors
{
public:
    static void setup(Stepper0 &stepper0, Stepper1 &stepper1);
    static void reset();
    /**
     * @brief Active or not
     *
     * @return true it at least one key is still being executed
     * @return false
     */
    static bool active();
    /**
     * @brief will inject stop keys
     *
     */
    static void request_stop();

    //
    static void process_begin_keys(const UartMessage *msg);
    static void process_add_keys(const UartKeysMessage *msg);
    static void process_end_keys(int slave_id, const UartEndKeysMessage *msg);

    // will execute the steps
    static void loop(Micros now);
};
