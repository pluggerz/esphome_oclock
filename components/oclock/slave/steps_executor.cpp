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

    const InflatedCmdKey &operator[](int idx) const
    {
        return cmds[idx];
    }

    InflatedCmdKey &operator[](int idx)
    {
        return cmds[idx];
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

 uint16_t reverse_steps = 5;

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
    bool speed_detection = false;
    bool speed_up = false;
    bool speed_down = false;

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

    inline bool fast_enough(int speed) __attribute__((always_inline))
    {
        return speed > stepperPtr->turn_speed_in_revs_per_minute;
    }

    inline bool do_speed_up() __attribute__((always_inline))
    {
        if (!speed_detection)
            // ignore
            return false;

        const auto &keys = *keysPtr;
        const auto &cur = keys[idx];
        const auto cur_speed = cmdSpeedUtil.deflate_speed(cur.inflated_speed());

        if (cur.ghost())
            // the stepper is 'not stepping'
            return false;
        if (idx == 0)
            // first command
            return fast_enough(cur_speed);
        const auto &prev = keys[idx - 1];
        if (prev.ghost())
            // threat as first command
            return fast_enough(cur_speed);
        if (prev.clockwise() == cur.clockwise())
            // same direction, the stepper will deal with it
            return false;
        // lets check if we need to 'start'
        return fast_enough(cur_speed);
    }

    inline bool do_speed_down() __attribute__((always_inline))
    {
        if (!speed_detection)
            return false;
        const auto &keys = *keysPtr;
        const auto &cur = keys[idx];
        const auto cur_speed = cmdSpeedUtil.deflate_speed(cur.inflated_speed());
        if (cur.ghost())
            // the stepper is 'not stepping'
            return false;

        if (idx + 1 >= MAX_ANIMATION_KEYS)
        {
            // last command, so lets check if we need to slow down
            return fast_enough(cur_speed);
        }
        const auto &nxt = keys[idx + 1];
        if (nxt.empty() || nxt.ghost())
            // last command, or next is still... lets check if we need to step down...
            return fast_enough(cur_speed);
        if (cur.clockwise() == nxt.clockwise())
            // same direction, the stepper will deal with it
            return false;
        // lets check if we need to 'stop'
        return fast_enough(cur_speed);
    }

    void loop(Micros now)
    {
        if (keysPtr == nullptr || stepperPtr == nullptr)
        {
            return;
        }

        auto &keys = *keysPtr;
        auto &stepper = *stepperPtr;
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

        stepper.disable_defecting();
        if (idx == keys.size())
        {
            // nothing to do
            keysPtr = nullptr;
            return;
        }
        const InflatedCmdKey &cmd = keys[idx];
        if (cmd.empty())
        {
            return;
        }
        const auto speed = cmdSpeedUtil.deflate_speed(cmd.inflated_speed());
        const auto clockwise = cmd.clockwise();
        if (turning == 0)
        {
            speed_up = do_speed_up();
            speed_down = do_speed_down();
#define STEPS keys[idx].value.steps
            if (speed_up && STEPS >= reverse_steps)
                STEPS -= reverse_steps;
            else
                speed_up = false;
            if (speed_down && STEPS >= reverse_steps)
                STEPS -= reverse_steps;
            else
                speed_down = false;
#undef STEPS
            turning = 3;
        }
        if (turning == 3)
        {
            turning--;
            if (speed_up)
            {
                stepper.setGhosting(false);
                stepper.enable_defecting(speed);
                steps = reverse_steps * STEP_MULTIPLIER;
                stepper.set_speed_in_revs_per_minute(clockwise ? speed : -speed, true);
            }
            return;
        }
        if (turning == 2)
        {
            turning--;
            // just execute
            const auto ghosting = cmd.ghost();
            stepper.setGhosting(ghosting);
            steps = cmd.steps() * STEP_MULTIPLIER;
            if (ghosting)
            {
                // we are standing 'still' so presume more speed
                stepper.set_current_speed_in_revs_per_minute(clockwise ? speed : -speed);
            }
            stepper.set_speed_in_revs_per_minute(clockwise ? speed : -speed);
            return;
        }
        if (turning == 1)
        {
            turning--;

            // execute
            if (speed_down)
            {
                stepper.setGhosting(false);
                stepper.enable_defecting(speed);
                steps = reverse_steps * STEP_MULTIPLIER;
                auto turn_speed = stepper.turn_speed_in_revs_per_minute;
                stepper.set_speed_in_revs_per_minute(clockwise ? turn_speed : -turn_speed);
            }
            idx++;
        }
    }

public:
    void stop()
    {
        keysPtr = nullptr;
    }

    void start(AnimationKeys *keys, u16 millisLeft, bool speed_detection)
    {
        this->keysPtr = keys;
        this->t0 = millis();
        this->millisLeft = millisLeft;
        this->idx = 0;
        this->steps = 0;
        this->turning = 0;
        this->speed_down = 0;
        this->speed_up = false;

        this->speed_detection = speed_detection;
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

void StepExecutors::process_end_keys(int slave_id, const UartEndKeysMessage *msg)
{
    cmdSpeedUtil.set_speeds(msg->speed_map);

    stepper0.speed_up = true;
    stepper1.speed_up = true;

    reverse_steps = msg->turn_speed_steps;

    stepper0.turn_speed_in_revs_per_minute = msg->turn_speed;
    stepper1.turn_speed_in_revs_per_minute = msg->turn_speed;

    bool speed_detection0 = msg->speed_detection & (1 << uint64_t(slave_id + 0));
    bool speed_detection1 = msg->speed_detection & (1 << uint64_t(slave_id + 1));

    animator0.start(&animationKeysArray[0], 1, speed_detection0);
    animator1.start(&animationKeysArray[1], 1, speed_detection1);
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