#include "oclock.h"

#ifdef MASTER_MODE

using namespace esphome;


#include "animation.h"
#include "ticks.h"

typedef std::function<int(int from, int to)> StepCalculator;

StepCalculator shortestPathCalculator = [](int from, int to)
{
    bool clockwise = Distance::clockwise(from, to) < Distance::antiClockwise(from, to);
    return clockwise ? Distance::clockwise(from, to) : -Distance::antiClockwise(from, to);
};

StepCalculator clockwiseCalculator = [](int from, int to)
{
    return Distance::clockwise(from, to);
};

StepCalculator antiClockwiseCalculator = [](int from, int to)
{
    return -Distance::antiClockwise(from, to);
};

void instructUsingStepCalculatorForHandle(Instructions &instructions, int speed, int handle_id, int to, const StepCalculator &calculator)
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

void instructUsingStepCalculator(Instructions &instructions, int speed, const HandlesState &goal, const StepCalculator &calculator)
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
        ESP_LOGD(TAG, "instructUsingSwipeWithBase: handle_id=%d max_steps=%d steps=%d ghost_steps=%d", handle_id, max_steps, steps, ghost_steps);
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

void instructUsingSwipe(Instructions &instructions, int speed, const HandlesState &goal, const StepCalculator &steps_calculator)
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
        ESP_LOGD(TAG, "For swipe: base_tick=%d -> max_steps=%d", base_tick, max_steps);
        if (max_steps < optimal_steps)
        {
            ESP_LOGD(TAG, "For swipe: new optimal: base_tick=%d -> max_steps=%d", base_tick, max_steps);
            optimal_steps = max_steps;
            optimal_base_tick = base_tick;
        }
    }
    ESP_LOGI(TAG, "For swipe: optimal_base_tick=%d", optimal_base_tick);
    instructUsingSwipeWithBase(instructions, speed, goal, steps_calculator, optimal_base_tick);
}
#endif