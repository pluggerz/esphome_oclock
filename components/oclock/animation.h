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
    int handleId;
    Cmd cmd;
    int orderId;
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
        DRAW_ROW2(0);
        DRAW_ROW(2);
        DRAW_ROW2(2);
        DRAW_ROW(4);
        DRAW_ROW2(4);
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

class Instructions : public HandlesState
{
public:
    std::vector<HandleCmd> cmds;

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
        int stepsOrPos = cmd.steps;
        const auto relativePosition = (cmd.mode & CmdEnum::ABSOLUTE) == 0;

        if (relativePosition && stepsOrPos == 0)
        {
            // ignore, since we are talking about steps
            return;
        }
        // calculate time
        timers[handle_id] += cmd.time();
        cmds.push_back(HandleCmd(handle_id, cmd, cmds.size()));
        const auto ghosting = (cmd.mode & CmdEnum::GHOST) != 0;
        if (ghosting)
            // stable ;P, just make sure time is aligned
            return;

        const auto clockwise = (cmd.mode & CmdEnum::CLOCKWISE) != 0;
        
        auto &current = tickz[handle_id];
        if (!relativePosition)
            current = Ticks::normalize(stepsOrPos);
        else if (current >= 0)
        {
            current += clockwise ? stepsOrPos : -stepsOrPos;
            current = Ticks::normalize(current);
        }
        else
        {
            ESP_LOGE(TAG, "Mmmm I think you did something wrong... hanlde_id=%d has no known state!?", handle_id);
        }
    };
};

typedef std::function<int(int from, int to)> StepCalculator;
extern StepCalculator shortestPathCalculator;
extern StepCalculator clockwiseCalculator;
extern StepCalculator antiClockwiseCalculator;

void instructUsingSwipe(Instructions &instructions, int speed, const HandlesState &goal, const StepCalculator &steps_calculator);
void instructUsingStepCalculator(Instructions &instructions, int speed, const HandlesState &goal, const StepCalculator &steps_calculator);
