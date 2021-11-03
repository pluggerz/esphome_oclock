#include "steps_executor.h"

class AnimationKeys
{
private:
    InflatedCmdKey cmds[MAX_ANIMATION_KEYS] = {};
    uint8_t idx = 0;

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
            cmds[idx++] = InflatedCmdKey::map(msg[i]);
    }
};

const uint16_t reverse_steps = 3;

template <class S>
class Animator final
{
public:
    Millis t0;
    Millis millisLeft;
    S *stepperPtr;
    AnimationKeys *keysPtr = nullptr;
    uint8_t idx = 0;
    uint8_t turning = 0;
    uint16_t steps = 0;

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
        S &stepper = *stepperPtr;
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

        const auto cmd = keys[idx];
        if (cmd.isEmpty())
        {
            return;
        }
        const auto clockwise = cmd.clockwise();
        const auto swap_speed = cmd.swap_speed();
        int eat_steps = 0;
        if (turning == 0)
        {
            stepper.set_defecting(false);

            auto was_swap_speed = idx > 0 && (keys[idx - 1].swap_speed());
            if (was_swap_speed)
            {
                // speed up
                auto steps_now = cmd.steps();
                auto steps_previous = keys[idx - 1].steps();
                eat_steps += min(reverse_steps, min(steps_now, steps_previous));
            }

            if (swap_speed)
            {
                turning = 2;

                // slow down
                auto steps_now = cmd.steps();
                auto steps_next = keys[idx + 1].steps();
                eat_steps += min(reverse_steps, min(steps_now, steps_next));
            }
            else
            {
                idx++;
            }
        }
        else if (turning == 2)
        {
            stepper.set_defecting(true);
            ESP_LOGD(TAG, "Sp down");
            --turning;

            steps = reverse_steps * STEP_MULTIPLIER;
            auto speed = stepper.turn_speed_in_revs_per_minute;
            stepper.set_speed_in_revs_per_minute(clockwise ? speed : -speed);

            ++idx;
            return;
        }
        else if (turning == 1)
        {
            ESP_LOGD(TAG, "Sp up %d", (int)cmd.speed());
            --turning;
            steps = reverse_steps * STEP_MULTIPLIER;
            stepper.set_speed_in_revs_per_minute(clockwise ? cmd.speed() : -cmd.speed());
            return;
        }
        const auto ghosting = cmd.ghost();
        stepper.setGhosting(ghosting);
        auto current = stepper.ticks();

        steps = (cmd.steps() - eat_steps) * STEP_MULTIPLIER;
        //TODO: should correct timings for 'eat_steps'
        ESP_LOGD(TAG, "E gh=%d s=%d cw=%d s=%d", (int)ghosting, (int)steps, (int)clockwise, (int)cmd.speed());

        stepper.set_speed_in_revs_per_minute(clockwise ? cmd.speed() : -cmd.speed());
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
        this->turning = 0;
        stepperPtr->sync();
    }
};

typedef Animator<Stepper0> Animator0;
typedef Animator<Stepper1> Animator1;

Animator0 animator0;
Animator1 animator1;

void StepExecutors::setup(Stepper0 &stepper0, Stepper1 &stepper1)
{
    animator0.stepperPtr = &stepper0;
    animator1.stepperPtr = &stepper1;
}

void StepExecutors::reset()
{
    animator0.stop();
    animator1.stop();
}

AnimationKeys animationKeysArray[2];

void StepExecutors::process_begin_keys(const UartMessage *msg)
{
    animator0.stop();
    animator1.stop();

    animationKeysArray[0].clear();
    animationKeysArray[1].clear();
}

void StepExecutors::process_end_keys(const UartEndKeysMessage *msg)
{
    stepper0.turn_speed_in_revs_per_minute = msg->turn_speed;
    stepper1.turn_speed_in_revs_per_minute = msg->turn_speed;

    cmdSpeedUtil.set_speeds(msg->speed_map);

    animator0.start(&animationKeysArray[0], 1);
    animator1.start(&animationKeysArray[1], 1);
}

void StepExecutors::process_add_keys(const UartKeysMessage *msg)
{
    animationKeysArray[msg->getDstId() & 1].addAll(*msg);
}

void StepExecutors::loop(Micros now)
{
    animator0.loop(now);
    animator1.loop(now);
}