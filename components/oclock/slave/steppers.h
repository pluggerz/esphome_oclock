#pragma once

#include "stepper.h"
#include "steps_executor.h"
#include "pins.h"

volatile int completedFirstHits = 2;
volatile int completedSecondHits = 2;
volatile int completedThirdHits = 2;

template <class S>
class PreMainMode final
{
private:
    S &stepper;

    // our administration
    bool hitFirstTime = true;
    bool hitSecondTime = true;
    bool hitThirdTime = true;

    int stepOutSteps = 0;
    static const int searchSpeed = 24;
    int initial_ticks_;

public:
    bool busy() const
    {
        return completedThirdHits != 2;
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
        if (hitFirstTime)
            --completedFirstHits;
        if (hitSecondTime)
            --completedSecondHits;
        if (hitThirdTime)
            --completedThirdHits;

        hitFirstTime = false;
        stepOutSteps = NUMBER_OF_STEPS / 6;
        hitSecondTime = false;
        hitThirdTime = false;

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
        if (hitThirdTime == true)
        {
            if (completedThirdHits == 2)
            {
                StepExecutors::loop(now);
            }
            return;
        }
        else if (hitFirstTime == false)
        {
            if (!stepper.is_magnet_tick())
            {
                return;
            }
            completedFirstHits++;
            hitFirstTime = true;
            stepper.set_speed_in_revs_per_minute(-searchSpeed / 3);
        }
        else if (stepOutSteps > 0)
        {
            if (completedFirstHits != 2)
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
        else if (hitSecondTime == false)
        {
            if (stepper.is_magnet_tick())
            {
                hitSecondTime = true;
                completedSecondHits++;
                stepper.set_speed_in_revs_per_minute(-searchSpeed / 1);
            }
        }
        else if (hitThirdTime == false)
        {
            if (stepper.ticks() == stepper.range(initial_ticks_))
            {
                hitThirdTime = true;
                completedThirdHits++;
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
