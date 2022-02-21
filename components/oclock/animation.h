#pragma once

#include "oclock.h"
#include "ticks.h"

using namespace esphome;

class AnimationController
{
    int16_t tickz[MAX_HANDLES] = {
        -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1};

    int clockId2animatorId[MAX_SLAVES] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
    int animatorId2clockId[MAX_SLAVES] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

public:
    void reset_handles()
    {
        for (int idx = 0; idx < MAX_HANDLES; ++idx)
            tickz[idx] = -1;
    }
    void set_handles(int slaveId, int pos0, int pos1)
    {
        auto animationId = clockId2animatorId[slaveId >> 1];
        if (animationId == -1)
        {
            ESP_LOGE(TAG, "S%d not mapped!?", slaveId);
            return;
        }
        auto handleId = animationId << 1;
        tickz[handleId] = pos0;
        tickz[handleId + 1] = pos1;
    }

    int getCurrentTicksForAnimatorHandleId(int animatorHandleId)
    {
        return animatorHandleId < 0 || animatorHandleId >= 48 ? -1 : tickz[animatorHandleId];
    }

    void dump_config(const char *tag)
    {
        ESP_LOGI(tag, "  animation_controller:");
        for (int idx = 0; idx < MAX_SLAVES; idx++)
        {
            auto animationId = clockId2animatorId[idx];
            if (animationId == -1)
                continue;
            auto handleId = animationId << 1;
            ESP_LOGI(tag, "   S%d -> A%d: offset(%d, %d)", idx, clockId2animatorId[idx],
                     tickz[handleId], tickz[handleId + 1]);
        }
    }

    void remap(int animator_id, int clockId)
    {
        animatorId2clockId[animator_id] = clockId;
        clockId2animatorId[clockId] = animator_id;
        ESP_LOGI(TAG, "added mapping: S%d <-> A%d", clockId, animator_id);
    }

    int mapAnimatorHandle2PhysicalHandleId(int animator_id)
    {
        int add_one = animator_id & 1;
        return add_one + (animatorId2clockId[animator_id >> 1] << 1);
    }

} extern animationController;

#include "keys.h"

class HandleCmd
{
public:
    inline bool ignorable() const
    {
        return handleId == -1;
    }
    int handleId;
    DeflatedCmdKey cmd;
    int orderId;
    inline uint8_t speed() const { return cmd.speed(); }
    inline bool ghost_or_alike() const
    {
        return cmd.ghost() || cmd.absolute();
    }
    HandleCmd() : handleId(-1), cmd(DeflatedCmdKey()), orderId(-1) {}
    HandleCmd(int handleId, DeflatedCmdKey cmd, int orderId) : handleId(handleId), cmd(cmd), orderId(orderId) {}
};

class Flags
{
private:
    uint64_t flags = 0xFFFFFFFFFFFFFFFF;

public:
    const uint64_t &raw() const
    {
        return flags;
    }

    void copyFrom(const Flags &src)
    {
        flags = src.flags;
    }

    bool operator[](int handleId) const
    {
        return ((flags >> handleId) & (uint64_t)1) == 1;
    }
    void hide(int handleId)
    {
        flags = flags & ~((uint64_t)1 << handleId);
    }
};

class HandlesState
{
protected:
    int16_t tickz[MAX_HANDLES] = {
        -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1};
    double timers[MAX_HANDLES] = {
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0};

public:
    Flags visibilityFlags, nonOverlappingFlags;

    double time_at(int handle_id) const { return timers[handle_id]; }

    inline bool valid_handle(int handleId) const
    {
        return tickz[handleId] >= 0l;
    }
    inline bool valid_handles(int handle_id_0, int handle_id_1) const
    {
        return valid_handle(handle_id_0) && valid_handle(handle_id_1);
    }

    void setAll(int longHandle, int shortHandle)
    {
        for (int idx = 0; idx < MAX_HANDLES; ++idx)
            tickz[idx] = 1 == (idx & 1) ? longHandle : shortHandle;
    }

    void setIds()
    {
        int m = NUMBER_OF_STEPS / 12;
        for (int idx = 0; idx < MAX_SLAVES; ++idx)
        {
            int handle1 = idx / 2;
            int handle2 = idx - handle1;
            tickz[idx * 2] = handle1 * m;
            tickz[idx * 2 + 1] = handle2 * m;
        }
    }

    void dump() const
    {
        ESP_LOGI(TAG, "HandlesState:");
#define DRAW_ROW(S)                                                                                                                                            \
    ESP_LOGI(TAG, "   A%02d(%3d %3d)    A%02d(%3d %3d)   A%02d(%3d %3d)   A%02d(%3d %3d)   A%02d(%3d %3d)   A%02d(%3d %3d)   A%02d(%3d %3d)   A%02d(%3d %3d)", \
             S + 0, tickz[(S + 0) * 2 + 0], tickz[(S + 0) * 2 + 1],                                                                                            \
             S + 1, tickz[(S + 1) * 2 + 0], tickz[(S + 1) * 2 + 1],                                                                                            \
             S + 6, tickz[(S + 6) * 2 + 0], tickz[(S + 6) * 2 + 1],                                                                                            \
             S + 7, tickz[(S + 7) * 2 + 0], tickz[(S + 7) * 2 + 1],                                                                                            \
             S + 12, tickz[(S + 12) * 2 + 0], tickz[(S + 12) * 2 + 1],                                                                                         \
             S + 13, tickz[(S + 13) * 2 + 0], tickz[(S + 13) * 2 + 1],                                                                                         \
             S + 18, tickz[(S + 18) * 2 + 0], tickz[(S + 18) * 2 + 1],                                                                                         \
             S + 19, tickz[(S + 19) * 2 + 0], tickz[(S + 19) * 2 + 1]);
#define DRAW_ROW2(S)                                                                                                                                           \
    ESP_LOGI(TAG, "      %3.2f %3.2f       %3.2f %3.2f      %3.2f %3.2f      %3.2f %3.2f      %3.2f %3.2f      %3.2f %3.2f      %3.2f %3.2f      %3.2f %3.2f", \
             timers[(S + 0) * 2 + 0], timers[(S + 0) * 2 + 1],                                                                                                 \
             timers[(S + 1) * 2 + 0], timers[(S + 1) * 2 + 1],                                                                                                 \
             timers[(S + 6) * 2 + 0], timers[(S + 6) * 2 + 1],                                                                                                 \
             timers[(S + 7) * 2 + 0], timers[(S + 7) * 2 + 1],                                                                                                 \
             timers[(S + 12) * 2 + 0], timers[(S + 12) * 2 + 1],                                                                                               \
             timers[(S + 13) * 2 + 0], timers[(S + 13) * 2 + 1],                                                                                               \
             timers[(S + 18) * 2 + 0], timers[(S + 18) * 2 + 1],                                                                                               \
             timers[(S + 19) * 2 + 0], timers[(S + 19) * 2 + 1]);

        DRAW_ROW(0);
        DRAW_ROW2(0);
        DRAW_ROW(2);
        DRAW_ROW2(2);
        DRAW_ROW(4);
        DRAW_ROW2(4);
#undef DRAW_ROW
        // INFO("visibilityFlags: " << std::bitset<MAX_HANDLES>(visibilityFlags.raw()));
        // INFO("nonOverlappingFlags: " << std::bitset<MAX_HANDLES>(nonOverlappingFlags.raw()));
    }

    void copyFrom(const HandlesState &src)
    {
        for (int idx = 0; idx < MAX_HANDLES; ++idx)
            tickz[idx] = src[idx];
        visibilityFlags.copyFrom(src.visibilityFlags);
        nonOverlappingFlags.copyFrom(src.nonOverlappingFlags);
    }

    void set_ticks(int handle_id, int ticks)
    {
        tickz[handle_id] = Ticks::normalize(ticks);
    }
    /*
    int16_t &operator[](int handleId)
    {
        return tickz[handleId];
    }*/
    const int16_t &operator[](int handleId) const
    {
        return tickz[handleId];
    }
};

#include <map>

class HandleBitMask
{
    uint64_t bits{0};
};

class Instructions : public HandlesState
{
public:
    static const bool send_relative;
    std::vector<HandleCmd> cmds;
    uint64_t speed_detection{~uint64_t(0)};
    static int turn_speed;
    static int turn_steps;

    void iterate_handle_ids(std::function<void(int handle_id)> func)
    {
        for (int handle_id = 0; handle_id < MAX_HANDLES; ++handle_id)
        {
            if (!valid_handle(handle_id))
            {
                continue;
            }
            func(handle_id);
        };
    }

    const uint64_t &get_speed_detection() const
    {
        return speed_detection;
    }

    void set_detect_speed_change(int animation_handle_id, bool value)
    {
        auto actual_handle_id = animationController.mapAnimatorHandle2PhysicalHandleId(animation_handle_id);
        if (actual_handle_id < 0)
        {
            // ignore
            ESP_LOGE(TAG, "Not mapped: animation_handle_id=%d", animation_handle_id);
            return;
        }

        // note: uint64_t is essential
        uint64_t bit = uint64_t(1) << uint64_t(actual_handle_id);
        if (value)
            speed_detection |= bit;
        else
            speed_detection &= ~bit;
        ESP_LOGE(TAG, "Mapped: animation_handle_id=%d -> actual_handle_id=%d and bit=%llu", animation_handle_id, actual_handle_id, speed_detection);
    }

    Instructions()
    {
        for (auto handleId = 0; handleId < MAX_HANDLES; handleId++)
            tickz[handleId] = animationController.getCurrentTicksForAnimatorHandleId(handleId);
    }

    void rejectInstructions(int firstHandleId, int secondHandleId)
    {
        std::for_each(cmds.begin(), cmds.end(), [firstHandleId, secondHandleId](HandleCmd &cmd)
                      {
                          if (cmd.handleId == firstHandleId || cmd.handleId == secondHandleId)
                              cmd.handleId = -1; });
    }

    void rejectInstructions(int handleId)
    {
        rejectInstructions(handleId, handleId);
    }

    void addAll(const DeflatedCmdKey &cmd)
    {
        for (int handleId = 0; handleId < MAX_HANDLES; ++handleId)
        {
            add(handleId, cmd);
        }
    }

    void add(int handle_id, const DeflatedCmdKey &cmd)
    {
        if (cmd.steps() == 0 && cmd.relative())
        {
            // ignore, since we are talking about steps
            return;
        }
        add_(handle_id, as_relative_cmd(handle_id, cmd));
    }

    inline DeflatedCmdKey as_relative_cmd(const int handle_id, const DeflatedCmdKey &cmd)
    {
        const auto relativePosition = cmd.relative();
        if (relativePosition)
            return cmd;

        // make relative
        const auto from_tick = tickz[handle_id];
        const auto to_tick = cmd.steps();
        return DeflatedCmdKey(
            cmd.mode() - CmdEnum::ABSOLUTE,
            cmd.clockwise() ? Distance::clockwise(from_tick, to_tick) : Distance::antiClockwise(from_tick, to_tick),
            cmd.speed());
    }

    void add(int handle_id, const CmdSpecialMode &mode)
    {
        auto cmd = DeflatedCmdKey(CmdEnum::ABSOLUTE, mode, 0);
        cmds.push_back(HandleCmd(handle_id, cmd, cmds.size()));
    }

    void follow_seconds(int handle_id, bool discrete)
    {
        add(handle_id, discrete ? CmdSpecialMode::FOLLOW_SECONDS_DISCRETE : CmdSpecialMode::FOLLOW_SECONDS);
    }

    /***
     * NOTE: cmd is relative
     */
    void add_(int handle_id, const DeflatedCmdKey &cmd)
    {
        // calculate time
        timers[handle_id] += cmd.time();
        cmds.push_back(HandleCmd(handle_id, cmd, cmds.size()));

        const auto ghosting = cmd.ghost();
        if (ghosting)
            // stable ;P, just make sure time is aligned
            return;

        auto &current = tickz[handle_id];
        if (current < 0)
        {
            ESP_LOGE(TAG, "Mmmm I think you did something wrong... hanlde_id=%d has no known state!?", handle_id);
        }
        current = Ticks::normalize(current + (cmd.clockwise() ? cmd.steps() : -cmd.steps()));
    };
};

class DistanceCalculators
{
public:
    typedef std::function<int(int from, int to)> Func;

    static Func shortest;
    static Func clockwise;
    static Func antiClockwise;

    static Func random()
    {
        std::vector<Func> options = {shortest, clockwise, antiClockwise};
        return options[::random(options.size())];
    }
};

class HandlesAnimations
{
public:
    typedef std::function<void(Instructions &instructions, int speed, const HandlesState &goal, const DistanceCalculators::Func &steps_calculator)> Func;

    static void instruct_using_swipe(Instructions &instructions, int speed, const HandlesState &goal, const DistanceCalculators::Func &steps_calculator);
    static void instructUsingStepCalculator(Instructions &instructions, int speed, const HandlesState &goal, const DistanceCalculators::Func &steps_calculator);

    static void instruct_using_random(Instructions &instructions, int speed, const HandlesState &goal, const DistanceCalculators::Func &steps_calculator)
    {
        std::vector<Func> options = {instruct_using_swipe, instructUsingStepCalculator};
        options[::random(options.size())](instructions, speed, goal, steps_calculator);
    }
};

class InBetweenAnimations
{
public:
    typedef std::function<void(Instructions &instructions, int speed)> Func;

    static void instructDelayUntilAllAreReady(Instructions &instructions, int speed, double additional_time = 0);

    static void instructNone(Instructions &instructions, int speed)
    {
        return;
    }

    static void instructStarAnimation(Instructions &instructions, int speed);
    static void instructDashAnimation(Instructions &instructions, int speed);
    static void instructMiddlePointAnimation(Instructions &instructions, int speed);
    static void instructAllInnerPointAnimation(Instructions &instructions, int speed);
    static void instructPacManAnimation(Instructions &instructions, int speed);

    static void instructRandom(Instructions &instructions, int speed)
    {
        std::vector<Func> options = {instructDashAnimation, instructMiddlePointAnimation, instructAllInnerPointAnimation, instructStarAnimation, instructPacManAnimation};
        options[::random(options.size())](instructions, speed);
    }
};