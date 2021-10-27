#pragma once

#include "interop.h"
#include "interop.keys.h"
#include "stepper.h"

class StepExecutors
{
public:
    static void setup(Stepper &stepper0, Stepper &stepper1);
 
    // 
    static void process_begin_keys(const UartMessage *msg);
    static void process_add_keys(const UartKeysMessage *msg);
    static void process_end_keys(const UartEndKeysMessage *msg);

    // will execute the steps
    static void loop(Micros now);
};
