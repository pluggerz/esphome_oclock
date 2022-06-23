#include "steps_executor.h"

enum class StepMode
{
    // we are speeding up the handle  honour the key request
    SPEED_UP,
    // executing key request
    CLOCKWISE,
    ANTI_CLOCKWISE,
    // we are speeding downs because we need to stop or slow down
    SPEED_DOWN,
};

class AnimationKeys
{
private:
    // SpecialFunc *func;
    InflatedCmdKey cmds[MAX_ANIMATION_KEYS] = {};
    uint8_t idx = 0;

public:
    void clear(uint8_t new_idx = 0)
    {
        idx = new_idx;
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
            cmds[idx++] = InflatedCmdKey(msg.get_key(i));
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
    StepMode step_mode = StepMode::SPEED_DOWN;
    uint16_t steps = 0;
    bool special = false;
    bool speed_detection = false;
    bool speed_up = false;
    bool speed_down = false;
    bool request_stop_ = false;
    // follow goal should be factored out...
    int follow_goal = -1;

    void request_stop()
    {
        request_stop_ = true;
        if (keysPtr == nullptr || stepperPtr == nullptr || special || stepperPtr->is_ghosting())
        {
            return;
        }
        if (step_mode == StepMode::CLOCKWISE || step_mode == StepMode::ANTI_CLOCKWISE)
        {
            // actually we do not care what the intended speed was
            fill_speed_down(step_mode == StepMode::CLOCKWISE, stepperPtr->turn_speed_in_revs_per_minute);
        }
    }

    void followSeconds(Micros now, bool discrete)
    {
        const auto current = stepperPtr->ticks();
        if (follow_goal < 0 || current == follow_goal)
        {
            Millis delta = now / 1000 - t0;
            if (delta > millisLeft)
            {
                follow_goal = 0;
            }
            else
            {
                double fraction = (double)delta / (double)millisLeft;
                if (discrete)
                    follow_goal = Ticks::normalize((double)NUMBER_OF_STEPS * ceil(60.0 * fraction) / 60.0);
                else
                    follow_goal = Ticks::normalize((double)NUMBER_OF_STEPS * fraction);
            }
        }
        if (follow_goal != current)
            stepperPtr->tryToStep(now);
    }

    inline bool fast_enough(int speed) __attribute__((always_inline))
    {
        return speed > stepperPtr->turn_speed_in_revs_per_minute;
    }

    /*inline*/ bool needs_speed_up() //__attribute__((always_inline))
    {
        if (!speed_detection)
            // ignore
            return false;

        const auto &keys = *keysPtr;
        const auto &cur = keys[idx];
        const auto cur_speed = cmdSpeedUtil.deflate_speed(cur.inflated_speed());

        if (cur.ghost_or_alike())
            // the stepper is 'not stepping'
            return false;
        if (idx == 0)
            // first command
            return fast_enough(cur_speed);
        const auto &prev = keys[idx - 1];
        if (prev.ghost_or_alike())
            // threat as first command
            return fast_enough(cur_speed);
        if (prev.clockwise() == cur.clockwise())
            // same direction, the stepper will deal with it
            return false;
        // lets check if we need to 'start'
        return fast_enough(cur_speed);
    }

    /*inline*/ bool needs_speed_down() // __attribute__((always_inline))
    {
        if (!speed_detection)
            return false;
        const auto &keys = *keysPtr;
        const auto &cur = keys[idx];
        const auto cur_speed = cmdSpeedUtil.deflate_speed(cur.inflated_speed());
        if (cur.ghost_or_alike())
            // the stepper is 'not stepping'
            return false;

        if (idx + 1 >= MAX_ANIMATION_KEYS || request_stop_)
        {
            // last command, so lets check if we need to slow down
            return fast_enough(cur_speed);
        }
        const auto &nxt = keys[idx + 1];
        if (nxt.empty() || nxt.ghost_or_alike())
            // last command, or next is still... lets check if we need to step down...
            return fast_enough(cur_speed);
        if (cur.clockwise() == nxt.clockwise())
            // same direction, the stepper will deal with it
            return false;
        // lets check if we need to 'stop'
        return fast_enough(cur_speed);
    }

    void fill_speed_down(bool clockwise, int speed)
    {
        auto &stepper = *stepperPtr;

        step_mode = StepMode::SPEED_DOWN;
        stepper.set_ghosting(false);
        stepper.enable_defecting(speed);
        steps = reverse_steps * STEP_MULTIPLIER;
        auto turn_speed = stepper.turn_speed_in_revs_per_minute;
        stepper.set_speed_in_revs_per_minute(clockwise ? turn_speed : -turn_speed);
    }

    void fill_speed_up(bool clockwise, int speed)
    {
        auto &stepper = *stepperPtr;

        step_mode = StepMode::SPEED_UP;
        stepper.set_ghosting(false);
        stepper.enable_defecting(speed);
        steps = reverse_steps * STEP_MULTIPLIER;
        stepper.set_speed_in_revs_per_minute(clockwise ? speed : -speed, true);
    }

    void loop(Micros now)
    {
        if (keysPtr == nullptr || stepperPtr == nullptr)
        {
            return;
        }

        auto &stepper = *stepperPtr;
        if (steps > 0)
        {
            if (special)
            {
                switch (steps)
                {
                case CmdSpecialMode::FOLLOW_SECONDS_DISCRETE:
                    followSeconds(now, true);
                    return;

                case CmdSpecialMode::FOLLOW_SECONDS:
                    followSeconds(now, false);
                    return;
                };
            }
            else
            {
                // still to do some work
                if (stepper.tryToStep(now))
                {
                    steps--;
                }
            }
            return;
        }

        auto &keys = *keysPtr;
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
            // nothing to do
            keysPtr = nullptr;
            return;
        }
        const auto speed = cmd.extended() ? 4 : cmdSpeedUtil.deflate_speed(cmd.inflated_speed());
        const auto clockwise = cmd.clockwise();
        step_mode = clockwise ? StepMode::CLOCKWISE : StepMode::ANTI_CLOCKWISE;
        if (turning == 0)
        {
            if (request_stop_)
            {
                // we are done!
                keysPtr = nullptr;
                return;
            }
            speed_up = needs_speed_up();
            speed_down = needs_speed_down();
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
            turning = 2;
            if (speed_up)
            {
                fill_speed_up(clockwise, speed);
            }
            return;
        }
        if (turning == 2)
        {
            turning--;
            // just execute
            const auto ghosting = cmd.ghost();
            stepper.set_ghosting(ghosting);
            special = cmd.extended();
            if (special)
            {
                steps = cmd.steps();
                follow_goal = -1;
                // should be adapted for the cases
                stepper.set_current_speed_in_revs_per_minute(8);
                stepper.set_speed_in_revs_per_minute(8);
            }
            else
            {
                steps = cmd.steps() * STEP_MULTIPLIER;
                if (ghosting)
                {
                    // we are standing 'still' so presume more speed
                    stepper.set_current_speed_in_revs_per_minute(clockwise ? speed : -speed);
                }
                stepper.set_speed_in_revs_per_minute(clockwise ? speed : -speed);
            }
            return;
        }
        if (turning == 1)
        {
            turning = 0;

            // execute
            if (speed_down)
            {
                fill_speed_down(clockwise, speed);
            }
            else if (request_stop_)
            {
                steps = 0;
            }
            idx++;
        }
    }

public:
    void stop()
    {
        keysPtr = nullptr;
    }

    bool active() const
    {
        return keysPtr != nullptr;
    }

    void start(AnimationKeys *keys, Millis millisLeft, bool speed_detection)
    {
        this->keysPtr = keys;
        this->t0 = ::millis();
        this->millisLeft = millisLeft;
        this->idx = 0;
        this->special = false;
        this->steps = 0;
        this->turning = 0;
        this->speed_down = false;
        this->speed_up = false;
        this->speed_detection = speed_detection;
        this->request_stop_ = false;

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

    reverse_steps = msg->turn_steps;

    stepper0.turn_speed_in_revs_per_minute = msg->turn_speed;
    stepper1.turn_speed_in_revs_per_minute = msg->turn_speed;

    bool speed_detection0 = msg->speed_detection & (1 << uint64_t(slave_id + 0));
    bool speed_detection1 = msg->speed_detection & (1 << uint64_t(slave_id + 1));

    animator0.start(&animationKeysArray[0], msg->number_of_millis_left, speed_detection0);
    animator1.start(&animationKeysArray[1], msg->number_of_millis_left, speed_detection1);
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

void StepExecutors::request_stop()
{
    animator0.request_stop();
    animator1.request_stop();
}

bool StepExecutors::active()
{
    return animator0.active() || animator1.active();
}
