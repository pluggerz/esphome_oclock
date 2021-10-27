#include "steps_executor.h"
#include "async.h"

class AnimationKeys
{
private:
    CmdInt cmds[MAX_ANIMATION_KEYS] = {};
    int idx = 0;

public:
    void clear()
    {
        idx = 0;
    }

    Cmd operator[](int idx) const
    {
        return Cmd(cmds[idx]);
    }

    int size() const
    {
        return idx;
    }

    void addAll(const AnimationKeys &keys)
    {
        for (int i = 0; i < keys.size(); ++i)
            cmds[idx++] = keys.cmds[i];
    }

    void addAll(const UartKeysMessage &msg)
    {
        for (int i = 0; i < msg.size(); ++i)
            cmds[idx++] = msg[i];
    }
};

class Animator final
{
public:
    Millis t0;
    Millis millisLeft;
    Stepper *stepperPtr;
    AnimationKeys *keysPtr = nullptr;
    bool do_sync{true};
    int idx = 0;
    int steps = 0;

    void followSeconds(Micros now, bool discrete)
    {
        Millis delta = now / 1000 - t0;
        if (delta > millisLeft)
            delta = millisLeft;
        double fraction = (double)delta / (double)millisLeft;
        int goal;
        if (discrete)
            goal = Ticks::normalize((double)NUMBER_OF_STEPS * ceil(60.0 * fraction) / 60.0);
        else
            goal = Ticks::normalize((double)NUMBER_OF_STEPS * fraction);
        int current = stepperPtr->ticks();
        if (goal != current)
            stepperPtr->tryToStep(now);
    }

    void loop(Micros now)
    {
        if (keysPtr == nullptr || stepperPtr == nullptr)
        {
            return;
        }

        AnimationKeys keys = *keysPtr;
        Stepper &stepper = *stepperPtr;
        if (steps > 0)
        {
            switch (steps)
            {
            case STEP_MULTIPLIER *CmdSpecialMode::FOLLOW_SECONDS_DISCRETE:
                followSeconds(now, true);
                return;

            case STEP_MULTIPLIER *CmdSpecialMode::FOLLOW_SECONDS:
                followSeconds(now, false);
                return;
            };

            // still to do some work
            if (stepper.tryToStep(now))
            {
                steps--;
            }
            return;
        }

        if (idx == keys.size())
        {
            // nothing to do
            keysPtr = nullptr;
            ESP_LOGI(TAG, "E done");
            return;
        }

        auto cmd = keys[idx++];
        if (cmd.isEmpty())
        {
            return;
        }
        const auto ghosting = (cmd.mode & CmdEnum::GHOST) != 0;
        stepper.setGhosting(ghosting);
        auto current = stepper.ticks();
        int goal = cmd.steps * STEP_MULTIPLIER;
        const auto clockwise = (cmd.mode & CmdEnum::CLOCKWISE) != 0;
        const auto relativePosition = (cmd.mode & CmdEnum::ABSOLUTE) == 0;
        ESP_LOGI(TAG, "E gh=%d g=%d cw=%d rp=%d s=%d", ghosting, goal, clockwise, relativePosition, cmd.speed);

        if (relativePosition)
            steps = goal;
        else
            steps = clockwise ? Distance::clockwise(current, goal) : Distance::antiClockwise(current, goal);

        stepper.setSpeed(clockwise ? cmd.speed : -cmd.speed);
        if (do_sync)
        {
            stepper.sync();
            do_sync = false;
        }
    }

public:
    void stop()
    {
        keysPtr = nullptr;
    }

    void start(AnimationKeys *keys, u16 millisLeft)
    {
        this->keysPtr = keys;
        ESP_LOGI(TAG, "K: %d", keys->size());
        this->t0 = millis();
        this->millisLeft = millisLeft;
        this->idx = 0;
        this->steps = 0;
        this->do_sync = true;
    }
} animator[2];

void StepExecutors::setup(Stepper &stepper0, Stepper &stepper1)
{
    animator[0].stepperPtr = &stepper0;
    animator[1].stepperPtr = &stepper1;
}

AnimationKeys animationKeysArray[2];

void StepExecutors::process_begin_keys(const UartMessage *msg)
{
    animator[0].stop();
    animator[1].stop();

    animationKeysArray[0].clear();
    animationKeysArray[1].clear();
}

void StepExecutors::process_end_keys(const UartEndKeysMessage *msg)
{
    cmdSpeedUtil.set_speeds(msg->speed_map);
    animator[0].start(&animationKeysArray[0], 1);
    animator[1].start(&animationKeysArray[1], 1);
}

void StepExecutors::process_add_keys(const UartKeysMessage *msg)
{
    animationKeysArray[msg->getDstId() & 1].addAll(*msg);
}

void StepExecutors::loop(Micros now)
{
    animator[0].loop(now);
    animator[1].loop(now);
}