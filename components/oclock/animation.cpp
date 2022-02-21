#include "oclock.h"

#ifdef MASTER_MODE

using namespace esphome;

#include "animation.h"
#include "ticks.h"
#include <cmath>

typedef std::function<int(int from, int to)> StepCalculator;

int Instructions::turn_speed{8};
int Instructions::turn_steps{5};

DistanceCalculators::Func DistanceCalculators::shortest = [](int from, int to)
{
    bool clockwise = Distance::clockwise(from, to) < Distance::antiClockwise(from, to);
    return clockwise ? Distance::clockwise(from, to) : -Distance::antiClockwise(from, to);
};

DistanceCalculators::Func DistanceCalculators::clockwise = [](int from, int to)
{
    return Distance::clockwise(from, to);
};

DistanceCalculators::Func DistanceCalculators::antiClockwise = [](int from, int to)
{
    return -Distance::antiClockwise(from, to);
};

void instructUsingStepCalculatorForHandle(Instructions &instructions, int speed, int handle_id, int to, const DistanceCalculators::Func &calculator)
{
    auto from = instructions[handle_id];
    auto steps = calculator(from, to);
    if (steps == 0)
    {
        // nothing to do ;P
        return;
    }
    auto clockwiseMode = steps > 0 ? CmdEnum::CLOCKWISE : CmdEnum::ANTI_CLOCKWISE;
    instructions.add(handle_id, DeflatedCmdKey(clockwiseMode | CmdEnum::ABSOLUTE, to, speed));
}

void HandlesAnimations::instructUsingStepCalculator(Instructions &instructions, int speed, const HandlesState &goal, const StepCalculator &calculator)
{
    for (int clock_id = 0; clock_id < MAX_SLAVES; ++clock_id)
    {
        auto handle0 = clock_id << 1;
        auto handle1 = handle0 + 1;

        auto from0 = instructions[handle0];
        auto from1 = instructions[handle1];
        if (!instructions.valid_handles(handle0, handle1))
        {
            // ignore
            ESP_LOGD(TAG, "S%d: handle_ids=(%d, %d) from=(%d, %d) is trash!",
                     clock_id, handle0, handle1, from0, from1);
            continue;
        }

        auto to0 = goal[handle0];
        auto to1 = goal[handle1];

        auto steps = abs(calculator(from0, to0)) + abs(calculator(from1, to1));
        auto steps_swapped = abs(calculator(from0, to1)) + abs(calculator(from1, to0));
        auto do_swap = steps > steps_swapped;
        ESP_LOGI(TAG, "S%d: from=(%d, %d) to=(%d, %d) steps=%d steps_swapped=%d -> do_swap=%s",
                 clock_id, from0, from1, to0, to1, steps, steps_swapped, YESNO(do_swap));

        if (do_swap)
        {
            instructUsingStepCalculatorForHandle(instructions, speed, handle0, to1, calculator);
            instructUsingStepCalculatorForHandle(instructions, speed, handle1, to0, calculator);
        }
        else
        {
            instructUsingStepCalculatorForHandle(instructions, speed, handle0, to0, calculator);
            instructUsingStepCalculatorForHandle(instructions, speed, handle1, to1, calculator);
        }
    }
}

void instructUsingSwipeWithBase(Instructions &instructions, int speed, const HandlesState &goal, const StepCalculator &steps_calculator, int base_tick)
{
    base_tick = Ticks::normalize(base_tick);

    int max_steps_from = 0;
    int max_steps_to = 0;
    for (int handle_id = 0; handle_id < MAX_HANDLES; ++handle_id)
    {
        if (!instructions.valid_handle(handle_id))
        {
            continue;
        }
        auto from = instructions[handle_id];
        max_steps_from = max(max_steps_from, abs(steps_calculator(from, base_tick)));

        auto to = goal[handle_id];
        max_steps_to = max(max_steps_to, abs(steps_calculator(base_tick, to)));
    }

    for (int handle_id = 0; handle_id < MAX_HANDLES; ++handle_id)
    {
        if (!instructions.valid_handle(handle_id))
        {
            continue;
        }
        int from = instructions[handle_id];

        // wait
        int steps = steps_calculator(from, base_tick);
        int ghost_steps = max_steps_from - abs(steps);
        ESP_LOGD(TAG, "instructUsingSwipeWithBase: handle_id=%d steps=%d ghost_steps=%d", handle_id, steps, ghost_steps);
        instructions.add(handle_id, DeflatedCmdKey(CmdEnum::GHOST, ghost_steps, speed));

        // go to base_tick
        instructions.add(handle_id, DeflatedCmdKey((steps >= 0 ? CLOCKWISE : ANTI_CLOCKWISE) | CmdEnum::RELATIVE, abs(steps), speed));

        // wait a sec
        instructions.add(handle_id, DeflatedCmdKey(CLOCKWISE | CmdEnum::RELATIVE | CmdEnum::GHOST, 100 / speed, speed));

        // go to final
        auto to = goal[handle_id];
        auto additional_steps = steps_calculator(base_tick, to);
        instructions.add(handle_id, DeflatedCmdKey((additional_steps >= 0 ? CLOCKWISE : ANTI_CLOCKWISE) | CmdEnum::RELATIVE, abs(additional_steps), speed));

        // wait
        ghost_steps = max_steps_to - abs(additional_steps);
        instructions.add(handle_id, DeflatedCmdKey(CmdEnum::GHOST, ghost_steps, speed));
    }
}

class ClockIdUtil
{
public:
    /***
     *        column:
     *           0  1    2   3     4   5   6   7
     * row  0 :  0  1    6   7    12  13  18  19
     *      1 :  2  3    8   9    14  15  20  21
     *      2 :  4  5   10  11    16  17  22  23
     */

    static int to_row_id(int clock_id)
    {
        return (clock_id % 6) >> 1;
    }

    static int to_column_id(int clock_id)
    {
#define CASE(CLOCK_ID, COLUMN) \
    case CLOCK_ID:             \
        return COLUMN;
        switch (clock_id)
        {
#define INNER_CASES(D) \
    CASE(D + 0, 0);    \
    CASE(D + 1, 1);    \
    CASE(D + 6, 2);    \
    CASE(D + 7, 3);    \
    CASE(D + 12, 4);   \
    CASE(D + 13, 5);   \
    CASE(D + 18, 6);   \
    CASE(D + 19, 7);
            INNER_CASES(0);
            INNER_CASES(2);
            INNER_CASES(4);
#undef INNER_CASES
#undef CASE
        default:
            ESP_LOGE(TAG, "Unable to map: clock_id=%d !?", clock_id);
            return 0;
        }
    }
};

class HandleIdUtil
{
public:
    static int to_clock_id(int handle_id)
    {
        return handle_id >> 1;
    };
    static int to_row_id(int handle_id)
    {
        return ClockIdUtil::to_row_id(to_clock_id(handle_id));
    }
    static int to_column_id(int handle_id)
    {
        return ClockIdUtil::to_column_id(to_clock_id(handle_id));
    }
};

int steps_needed_for_given_time_and_speed(double time, int speed)
{
    return time * double(speed * NUMBER_OF_STEPS) / 60.0d;
};

void InBetweenAnimations::instructDelayUntilAllAreReady(Instructions &instructions, int speed, double additional_time)
{
    double max_time = -1;
    for (int handle_id = 0; handle_id < MAX_HANDLES; ++handle_id)
    {
        if (!instructions.valid_handle(handle_id))
        {
            continue;
        }
        auto time = instructions.time_at(handle_id);
        max_time = max_time < 0 ? time : max(time, max_time);
    }
    if (max_time < 0)
        // no valid handles?
        return;
    max_time += additional_time;
    for (int handle_id = 0; handle_id < MAX_HANDLES; ++handle_id)
    {
        if (!instructions.valid_handle(handle_id))
        {
            continue;
        }
        auto time = max_time - instructions.time_at(handle_id);
        auto steps = steps_needed_for_given_time_and_speed(time, speed);
        if (steps > 0)
            instructions.add(handle_id, DeflatedCmdKey(GHOST | RELATIVE, steps, speed));
    }
}

void InBetweenAnimations::instructStarAnimation(Instructions &instructions, int speed)
{
    int start_array[4] = {270,
                          450,
                          630,
                          90};
    // firs all go to our goal
    HandlesState start;
    for (int handle_id = 0; handle_id < MAX_HANDLES; ++handle_id)
    {
        if (!instructions.valid_handle(handle_id))
        {
            continue;
        }

        auto row_id = HandleIdUtil::to_row_id(handle_id) % 2;
        auto column_id = HandleIdUtil::to_column_id(handle_id) % 2;
        auto case_id = 2 * row_id + row_id;
        start.set_ticks(handle_id, start_array[case_id]);
    };
    HandlesAnimations::instructUsingStepCalculator(instructions, speed, start, DistanceCalculators::shortest);
    instructDelayUntilAllAreReady(instructions, speed, 2.0d);
    for (int handle_id = 0; handle_id < MAX_HANDLES; ++handle_id)
    {
        if (!instructions.valid_handle(handle_id))
        {
            continue;
        }
        bool clockwise = handle_id % 2;
        auto steps = NUMBER_OF_STEPS - NUMBER_OF_STEPS;
        instructions.add(handle_id, DeflatedCmdKey(RELATIVE | (clockwise ? CLOCKWISE : ANTI_CLOCKWISE), steps, speed));
        instructions.add(handle_id, DeflatedCmdKey(RELATIVE | (!clockwise ? CLOCKWISE : ANTI_CLOCKWISE), steps / 2, speed));
    }
    instructDelayUntilAllAreReady(instructions, speed, 2.0d);
}

void InBetweenAnimations::instructPacManAnimation(Instructions &instructions, int speed)
{
    HandlesState start;
    instructions.iterate_handle_ids(
        [&](int handle_id)
        {
            switch (HandleIdUtil::to_row_id(handle_id))
            {
            case 0:
                start.set_ticks(handle_id, 0);
                break;
            case 1:
                start.set_ticks(handle_id, handle_id % 2 ? 0 : NUMBER_OF_STEPS / 2);
                break;
            case 2:
                start.set_ticks(handle_id, NUMBER_OF_STEPS / 2);
                break;
            }
        });
    // lets go there
    HandlesAnimations::instructUsingStepCalculator(instructions, speed, start, DistanceCalculators::shortest);
    // some delay
    instructDelayUntilAllAreReady(instructions, speed, 2.0d);
    const int randomness[2] = {NUMBER_OF_STEPS / 2 + random(NUMBER_OF_STEPS / 2), NUMBER_OF_STEPS / 2 + random(NUMBER_OF_STEPS / 4)};
    instructions.iterate_handle_ids(
        [&](int handle_id)
        {
            auto forward = handle_id % 2 ? CLOCKWISE : ANTI_CLOCKWISE;
            auto reverse = handle_id % 2 ? ANTI_CLOCKWISE : CLOCKWISE;
            instructions.add(handle_id, DeflatedCmdKey(RELATIVE | forward, NUMBER_OF_STEPS, speed));
            instructions.add(handle_id, DeflatedCmdKey(RELATIVE | reverse, NUMBER_OF_STEPS / 2 + randomness[0], speed));
            instructions.add(handle_id, DeflatedCmdKey(RELATIVE | forward, NUMBER_OF_STEPS / 2 + randomness[1], speed));
        });
}

auto origin_function = [](int handle_id)
{
    auto y = double(HandleIdUtil::to_row_id(handle_id)) - 1.0d;
    auto x = double(HandleIdUtil::to_column_id(handle_id)) - 3.5d;
    auto principal = atan2(y, x);
    return double(NUMBER_OF_STEPS / 4) + principal * double(NUMBER_OF_STEPS) / 2.0d / 3.14159265359d;
};

void InBetweenAnimations::instructAllInnerPointAnimation(Instructions &instructions, int speed)
{
    HandlesState start;
    instructions.iterate_handle_ids(
        [&](int handle_id)
        { start.set_ticks(handle_id, origin_function(handle_id) + NUMBER_OF_STEPS / 2); });
    // lets go there
    HandlesAnimations::instructUsingStepCalculator(instructions, speed, start, DistanceCalculators::shortest);
    // some delay
    instructDelayUntilAllAreReady(instructions, speed, 2.0d);
    const int randomness[2] = {NUMBER_OF_STEPS / 2 + random(NUMBER_OF_STEPS / 2), NUMBER_OF_STEPS / 2 + random(NUMBER_OF_STEPS / 4)};
    instructions.iterate_handle_ids(
        [&](int handle_id)
        {
            auto steps = NUMBER_OF_STEPS;
            instructions.add(handle_id, DeflatedCmdKey(RELATIVE | CLOCKWISE, steps, speed));
            instructions.add(handle_id, DeflatedCmdKey(RELATIVE | ANTI_CLOCKWISE, randomness[0], speed));
            instructions.add(handle_id, DeflatedCmdKey(RELATIVE | CLOCKWISE, randomness[1], speed));
        });
}

void InBetweenAnimations::instructMiddlePointAnimation(Instructions &instructions, int speed)
{
    HandlesState start;
    instructions.iterate_handle_ids(
        [&](int handle_id)
        { start.set_ticks(handle_id, origin_function(handle_id)); });
    // lets go there
    HandlesAnimations::instructUsingStepCalculator(instructions, speed, start, DistanceCalculators::shortest);
    // some delay
    instructDelayUntilAllAreReady(instructions, speed, 2.0d);
    instructions.iterate_handle_ids(
        [&](int handle_id)
        {
            auto clockwise = handle_id % 2;
            auto steps = NUMBER_OF_STEPS;
            instructions.add(handle_id, DeflatedCmdKey(RELATIVE | (clockwise ? CLOCKWISE : ANTI_CLOCKWISE), steps, speed));
            instructions.add(handle_id, DeflatedCmdKey(RELATIVE | (!clockwise ? CLOCKWISE : ANTI_CLOCKWISE), steps, speed));
        });
    instructDelayUntilAllAreReady(instructions, speed, 2.0d);
};

void InBetweenAnimations::instructDashAnimation(Instructions &instructions, int speed)
{
    auto case_func = [](int handle_id)
    {
        auto row_id = HandleIdUtil::to_row_id(handle_id);
        switch (row_id)
        {
        case 0:
            return 0;

        case 2:
            return 1;

        default:
            return handle_id % 2;
        }
    };

    // lets determine the initial position
    HandlesState start;
    instructions.iterate_handle_ids(
        [&](int handle_id)
        { start.set_ticks(handle_id, case_func(handle_id) == 0 ? NUMBER_OF_STEPS / 2 : 0); });
    // lets go there
    HandlesAnimations::instructUsingStepCalculator(instructions, speed, start, DistanceCalculators::shortest);
    // some delay
    instructDelayUntilAllAreReady(instructions, speed, 2.0d);
    // our animation
    const int randomness[3] = {random(NUMBER_OF_STEPS / 4), random(NUMBER_OF_STEPS / 4), random(NUMBER_OF_STEPS / 4)};
    instructions.iterate_handle_ids(
        [&](int handle_id)
        {
            for (auto idx = 0; idx < 3; ++idx)
            {
                auto steps = NUMBER_OF_STEPS / 4 + randomness[idx];
                instructions.add(handle_id, DeflatedCmdKey(RELATIVE | CLOCKWISE, steps, speed));
                instructions.add(handle_id, DeflatedCmdKey(RELATIVE | ANTI_CLOCKWISE, steps * 3 / 4, speed));
                instructions.add(handle_id, DeflatedCmdKey(RELATIVE | CLOCKWISE, steps / 2, speed));
            }
        });
    instructDelayUntilAllAreReady(instructions, speed, 2.0d);
}

void HandlesAnimations::instruct_using_swipe(Instructions &instructions, int speed, const HandlesState &goal, const StepCalculator &steps_calculator)
{
    // find shortes path
    int optimal_base_tick = 0;
    int optimal_steps = 2 * NUMBER_OF_STEPS;

    for (int base_tick = 0; base_tick < NUMBER_OF_STEPS; ++base_tick)
    {
        int max_steps = 0;
        for (int handle_id = 0; handle_id < MAX_HANDLES; ++handle_id)
        {
            if (!instructions.valid_handle(handle_id))
            {
                continue;
            }
            auto from = instructions[handle_id];
            auto to = goal[handle_id];

            auto steps = abs(steps_calculator(from, base_tick)) + abs(steps_calculator(base_tick, to));
            max_steps = max_steps > steps ? max_steps : steps;
            if (max_steps > optimal_steps)
            {
                // no need to check further
                break;
            }
        }
        ESP_LOGVV(TAG, "For swipe: base_tick=%d -> max_steps=%d", base_tick, max_steps);
        if (max_steps < optimal_steps)
        {
            ESP_LOGVV(TAG, "For swipe: new optimal: base_tick=%d -> max_steps=%d", base_tick, max_steps);
            optimal_steps = max_steps;
            optimal_base_tick = base_tick;
        }
    }
    ESP_LOGI(TAG, "For swipe: optimal_base_tick=%d", optimal_base_tick);
    instructUsingSwipeWithBase(instructions, speed, goal, steps_calculator, optimal_base_tick);
}
#endif