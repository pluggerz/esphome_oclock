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
    Cmd cmd;
    int orderId;
    inline uint8_t speed() const { return cmd.speed(); }
    HandleCmd() : handleId(-1), cmd(Cmd()), orderId(-1) {}
    HandleCmd(int handleId, Cmd cmd, int orderId) : handleId(handleId), cmd(cmd), orderId(orderId) {}
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
        //DRAW_ROW2(0);
        DRAW_ROW(2);
        //DRAW_ROW2(2);
        DRAW_ROW(4);
        //DRAW_ROW2(4);
#undef DRAW_ROW
        //INFO("visibilityFlags: " << std::bitset<MAX_HANDLES>(visibilityFlags.raw()));
        //INFO("nonOverlappingFlags: " << std::bitset<MAX_HANDLES>(nonOverlappingFlags.raw()));
    }

    void copyFrom(const HandlesState &src)
    {
        for (int idx = 0; idx < MAX_HANDLES; ++idx)
            tickz[idx] = src[idx];
        visibilityFlags.copyFrom(src.visibilityFlags);
        nonOverlappingFlags.copyFrom(src.nonOverlappingFlags);
    }

    int16_t &operator[](int handleId)
    {
        return tickz[handleId];
    }
    const int16_t &operator[](int handleId) const
    {
        return tickz[handleId];
    }
};

#include <map>

class Instructions : public HandlesState
{
public:
    static const bool send_relative;
    std::vector<HandleCmd> cmds;
    std::map<int, int> last_idx_cmd_by_handle_id;
    bool swap_speed_detection = true;
    const int turn_speed{8};

    Instructions()
    {
        for (int handleId = 0; handleId < MAX_HANDLES; handleId++)
            tickz[handleId] = animationController.getCurrentTicksForAnimatorHandleId(handleId);
        dump();
    }

    void rejectInstructions(int firstHandleId, int secondHandleId)
    {
        std::for_each(cmds.begin(), cmds.end(), [firstHandleId, secondHandleId](HandleCmd &cmd)
                      {
                          if (cmd.handleId == firstHandleId || cmd.handleId == secondHandleId)
                              cmd.handleId = -1;
                      });
    }

    void rejectInstructions(int handleId)
    {
        rejectInstructions(handleId, handleId);
    }

    void addAll(const Cmd &cmd)
    {
        for (int handleId = 0; handleId < MAX_HANDLES; ++handleId)
        {
            add(handleId, cmd);
        }
    }

    void add(int handle_id, const Cmd &cmd)
    {
        if (cmd.steps() == 0 && cmd.relative())
        {
            // ignore, since we are talking about steps
            return;
        }

        if (!send_relative)
            add_postprocess(handle_id, as_absolute_cmd(handle_id, cmd), false);
        else
            add_postprocess(handle_id, as_relative_cmd(handle_id, cmd), true);
    }

    inline Cmd as_relative_cmd(const int handle_id, const Cmd &cmd)
    {
        const auto relativePosition = cmd.relative();
        if (relativePosition)
            return cmd;

        // make relative
        const auto from_tick = tickz[handle_id];
        const auto to_tick = cmd.steps();
        return Cmd(
            cmd.mode() - CmdEnum::ABSOLUTE,
            cmd.clockwise() ? Distance::clockwise(from_tick, to_tick) : Distance::antiClockwise(from_tick, to_tick),
            cmd.speed());
    }

    void add_postprocess(int handle_id, const Cmd &cmd, bool relative)
    {
        auto key = last_idx_cmd_by_handle_id.find(handle_id);
        if (key == last_idx_cmd_by_handle_id.end())
        {
            add_(handle_id, cmd, relative);
            return;
        }
        Cmd &last_cmd = cmds[key->second].cmd;
        auto last_direction = last_cmd.clockwise();
        auto direction = cmd.clockwise();

        if (last_direction == direction)
        {
            add_(handle_id, cmd, relative);
            return;
        }
        if (max(last_cmd.speed(), cmd.speed()) <= turn_speed)
        {
            // same direction or we are not that fast...
            add_(handle_id, cmd, relative);
            return;
        }

        auto last_ghosting = last_cmd.ghost();
        auto ghosting = cmd.ghost();
        if (!last_ghosting && !ghosting && swap_speed_detection)
        {
            last_cmd.value.modeKey.mode |= CmdEnum::SWAP_SPEED;
        }
        add_(handle_id, cmd, relative);
    }

    inline Cmd as_absolute_cmd(const int handle_id, const Cmd &cmd)
    {
        const auto relativePosition = cmd.relative();
        if (!relativePosition)
        {
            // remove the flag, and normalize the steps to be sure
            return Cmd(
                cmd.mode() - CmdEnum::ABSOLUTE,
                Ticks::normalize(cmd.steps()),
                cmd.speed());
        }
        if (cmd.steps() > NUMBER_OF_STEPS)
        {
            ESP_LOGE(TAG, "Unable to convert relative to absolute since cmd.steps=%d", cmd.steps());
        }
        // make absolute
        return Cmd(
            cmd.mode(),
            Ticks::normalize(tickz[handle_id] + (cmd.clockwise() ? cmd.steps() : -cmd.steps())),
            cmd.speed());
    }

    /***
     * NOTE: cmd is relative
     */
    void add_(int handle_id, const Cmd &cmd, bool relative)
    {
        // calculate time
        timers[handle_id] += cmd.time();
        last_idx_cmd_by_handle_id[handle_id] = cmds.size();
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
        else if (relative)
        {
            current = Ticks::normalize(current + (cmd.clockwise() ? cmd.steps() : -cmd.steps()));
        }
        else
        {
            current = cmd.steps();
        }
    };
};

typedef std::function<int(int from, int to)> StepCalculator;
extern StepCalculator shortestPathCalculator;
extern StepCalculator clockwiseCalculator;
extern StepCalculator antiClockwiseCalculator;

void instructUsingSwipe(Instructions &instructions, int speed, const HandlesState &goal, const StepCalculator &steps_calculator);
void instructUsingStepCalculator(Instructions &instructions, int speed, const HandlesState &goal, const StepCalculator &steps_calculator);
