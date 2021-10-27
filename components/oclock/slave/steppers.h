#include "stepper.h"
#include "Arduino.h"
#include "steps_executor.h"

volatile int completedFirstHits = 2;
volatile int completedSecondHits = 2;
volatile int completedThirdHits = 2;

class PreMainMode final 
{
private:
    const String name;
    Stepper &stepper;

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
        stepper.setSpeed(+searchSpeed);
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
            if (stepper.findZero() != 0)
            {
                return;
            }
            completedFirstHits++;
            hitFirstTime = true;
            stepper.setSpeed(-searchSpeed / 3);
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
                stepper.setSpeed(+searchSpeed / 1);
            }
        }
        else if (hitSecondTime == false)
        {
            if (stepper.findZero() == 0)
            {
                hitSecondTime = true;
                completedSecondHits++;
                stepper.setSpeed(-searchSpeed / 1);
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
    PreMainMode(Stepper &stepper, const String &name, int initialTicks)
        : name(name),
          stepper(stepper),
          initial_ticks_(initialTicks)
    {
        reset();
    }
};
