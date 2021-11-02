#pragma once

#include "stepper.h"
#include "steps_executor.h"
#include "pins.h"

struct Completed
{
    // lets  save some space
    uint8_t FirstHits : 2;
    uint8_t SecondHits : 2;
    uint8_t ThirdHits : 2;

    Completed()
    {
        FirstHits = 2;
        SecondHits = 2;
        ThirdHits = 2;
    }
} volatile completed;

struct Hit
{
    // lets  save some space
    uint8_t FirstTime : 1;
    uint8_t SecondTime : 1;
    uint8_t ThirdTime : 1;

    Hit()
    {
        FirstTime = true;
        SecondTime = true;
        ThirdTime = true;
    }
};

template <class S>
class PreMainMode final
{
private:
    // our administration
    Hit hit;
    static const uint8_t searchSpeed = 20;
    int16_t stepOutSteps = 0;
    S &stepper;
    int16_t initial_ticks_;
    
public:
    bool busy() const
    {
        return completed.ThirdHits != 2;
    }

    bool set_initial_ticks(int value)
    {
        if (initial_ticks_ == value)
            return false;
        initial_ticks_ = value;
        return true;
    }

    void reset()
    {
        if (hit.FirstTime)
            --completed.FirstHits;
        if (hit.SecondTime)
            --completed.SecondHits;
        if (hit.ThirdTime)
            --completed.ThirdHits;

        hit.FirstTime = false;
        stepOutSteps = NUMBER_OF_STEPS / 6;
        hit.SecondTime = false;
        hit.ThirdTime = false;

        // always from the same side, to make sure we stop the same way
        stepper.set_speed_in_revs_per_minute(+searchSpeed);
        stepper.sync();
    }

    void stop()
    {
    }

    void setup(Micros now)
    {
        reset();
    }

    void loop(Micros now)
    {
        //
        if (hit.ThirdTime == true)
        {
            if (completed.ThirdHits == 2)
            {
                StepExecutors::loop(now);
            }
            return;
        }
        else if (hit.FirstTime == false)
        {
            if (!stepper.is_magnet_tick())
            {
                return;
            }
            completed.FirstHits++;
            hit.FirstTime = true;
            stepper.set_speed_in_revs_per_minute(-searchSpeed / 3);
        }
        else if (stepOutSteps > 0)
        {
            if (completed.FirstHits != 2)
            {
                return;
            }
            if (stepper.tryToStep(now))
            {
                stepOutSteps--;
            }
            if (stepOutSteps == 0)
            {
                stepper.set_speed_in_revs_per_minute(+searchSpeed / 1);
            }
        }
        else if (hit.SecondTime == false)
        {
            if (stepper.is_magnet_tick())
            {
                hit.SecondTime = true;
                completed.SecondHits++;
                stepper.set_speed_in_revs_per_minute(-searchSpeed / 1);
            }
        }
        else if (hit.ThirdTime == false)
        {
            if (stepper.ticks() == stepper.range(initial_ticks_))
            {
                hit.ThirdTime = true;
                stepper.sync();
                completed.ThirdHits++;
            }
            else
            {
                stepper.tryToStep(now);
            }
        }
    }

public:
    PreMainMode(S &stepper, int initialTicks)
        : stepper(stepper),
          initial_ticks_(initialTicks)
    {
        reset();
    }
};
