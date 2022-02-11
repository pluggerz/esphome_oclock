#pragma once

#include <set>

#include "interop.h"
#include "master.h"
#include "animation.h"
#include "handles.h"
#include "time_tracker.h"

#include "interop.keys.h"

namespace oclock
{
    namespace requests
    {

        void background_brightness(const float value);
        void background_color(const int component, const float value);

        class AnimationRequest : public oclock::BroadcastRequest
        {
        protected:
            void sendCommandsForHandle(int animatorHandleId, const std::vector<DeflatedCmdKey> &commands)
            {
                auto physicalHandleId = animationController.mapAnimatorHandle2PhysicalHandleId(animatorHandleId);
                if (physicalHandleId < 0 || commands.empty())
                    // ignore
                    return;

                auto nmbrOfCommands = commands.size();
                UartKeysMessage msg(physicalHandleId, (u8)nmbrOfCommands);
                for (std::size_t idx = 0; idx < nmbrOfCommands; ++idx)
                {
                    msg[idx] = commands[idx].asInflatedCmdKey().raw;
                }

                if (true)
                {
                    ESP_LOGI(TAG, "send(S%02d, A%d->PA%d size: %d",
                             animatorHandleId >> 1, animatorHandleId, physicalHandleId, commands.size());

                    if (false)
                    {
                        for (std::size_t idx = 0; idx < nmbrOfCommands; ++idx)
                        {
                            const auto &cmd = InflatedCmdKey::map(msg[idx]);
                            if (cmd.extended())
                            {
                                const auto extended_cmd = cmd.steps();
                                ESP_LOGI(TAG, "Cmd: extended_cmd=%d", int(extended_cmd));
                            }
                            else
                            {
                                const auto ghosting = cmd.ghost();
                                const auto steps = cmd.steps();
                                const auto speed = cmdSpeedUtil.deflate_speed(cmd.inflated_speed());
                                const auto clockwise = cmd.clockwise();
                                const auto relativePosition = true;
                                ESP_LOGI(TAG, "Cmd: %s=%3d sp=%d gh=%s cl=%s", ghosting || relativePosition ? "steps" : "   to", steps, speed, YESNO(ghosting), ghosting ? "N/A" : YESNO(clockwise));
                            }
                        }
                        ESP_LOGI(TAG, "  done");
                    }
                }
                send(msg);
            }

            void sendAndClear(int handleId, std::vector<DeflatedCmdKey> &selected)
            {
                if (selected.size() > 0)
                {
                    sendCommandsForHandle(handleId, selected);
                    selected.clear();
                }
            }

            void sendCommands(std::vector<HandleCmd> &cmds)
            {
                std::sort(std::begin(cmds), std::end(cmds), [](const HandleCmd &a, const HandleCmd &b)
                          { return a.handleId == b.handleId ? a.orderId < b.orderId : a.handleId < b.handleId; });
                std::vector<DeflatedCmdKey> selected;
                int lastHandleId = -1;
                int lastHandleMessages = 0;
                for (auto it = std::begin(cmds); it != std::end(cmds); ++it)
                {
                    const auto &handleCmd = *it;
                    if (handleCmd.ignorable())
                    {
                        // ignore ('deleted')
                        continue;
                    }
                    if (handleCmd.handleId != lastHandleId)
                    {
                        sendAndClear(lastHandleId, selected);
                        ESP_LOGW(TAG, "H%d : %d messages", lastHandleId, lastHandleMessages);

                        lastHandleId = handleCmd.handleId;
                        lastHandleMessages = 0;
                    }
                    lastHandleMessages++;
                    selected.push_back(handleCmd.cmd);
                    if (selected.size() == MAX_ANIMATION_KEYS_PER_MESSAGE)
                    {
                        sendAndClear(lastHandleId, selected);
                    }
                }
                sendAndClear(lastHandleId, selected);
                ESP_LOGW(TAG, "H%d : %d messages", lastHandleId, lastHandleMessages);
                cmds.clear();
            }

            static void copyTo(const ClockCharacters &chars, HandlesState &state)
            {
                auto lambda = [&state](int handleId, int hours)
                {
                    int goal = static_cast<int>(NUMBER_OF_STEPS * static_cast<double>(hours == 13 ? 7.5 : (double)hours) / 12.0);
                    state.set_ticks(handleId, goal);
                    if (hours == 13)
                    {
                        state.visibilityFlags.hide(handleId);
                    }
                };
                ClockUtil::iterate_handles(chars, lambda);
                for (int handleId = 0; handleId < MAX_HANDLES; handleId++)
                {
                    if (state[handleId] == state[handleId + 1])
                        state.nonOverlappingFlags.hide(handleId);
                }
                // state.debug();
            }

            void updateSpeeds(const Instructions &instructions)
            {
                // gather speeds
                std::set<int> new_speeds_as_set{1};
                for (const auto &cmd : instructions.cmds)
                {
                    if (cmd.ignorable() || cmd.ghost_or_alike())
                        continue;
                    new_speeds_as_set.insert(cmd.speed());
                }

                if (new_speeds_as_set.size() > cmdSpeedUtil.max_inflated_speed)
                {
                    ESP_LOGE(TAG, "Too many speeds to process !? current %d, while max %d",
                             new_speeds_as_set.size(), cmdSpeedUtil.max_inflated_speed);
                    return;
                }

                CmdSpeedUtil::Speeds speeds;
                int last_speed = 1;
                int idx = 0;
                for (auto speed : new_speeds_as_set)
                {
                    speeds[idx++] = speed;
                    last_speed = speed;
                }
                for (; idx <= cmdSpeedUtil.max_inflated_speed; ++idx)
                {
                    speeds[idx] = last_speed;
                }
                cmdSpeedUtil.set_speeds(speeds);
            }

            void sendInstructions(Instructions &instructions, u32 millisLeft = u32(-1))
            {
                updateSpeeds(instructions);

                // lets start transmitting
                send(UartMessage(-1, MsgType::MSG_BEGIN_KEYS));

                // send instructions
                sendCommands(instructions.cmds);
                instructions.dump();
                // finalize
                send(UartEndKeysMessage(
                    instructions.turn_speed,
                    instructions.turn_speed_steps,
                    cmdSpeedUtil.get_speeds(),
                    instructions.get_speed_detection(),
                    millisLeft));
                ESP_LOGI(TAG, "millisLeft=%ld", long(millisLeft));
            }

        public:
            virtual void execute() override final
            {
                animationController.reset_handles();
                send(UartPosRequest(false));
            }
        };

        class SpeedAdaptTestRequest final : public AnimationRequest
        {
        public:
            virtual void finalize() override final
            {
                Instructions instructions;
                auto speed = oclock::master.get_base_speed();
                instructions.add(20 * 2 + 0, DeflatedCmdKey(CLOCKWISE | RELATIVE, 720, speed));
                for (int step = 0; step < 30; ++step)
                    instructions.add(20 * 2 + 1, DeflatedCmdKey(CLOCKWISE | RELATIVE, 24, speed));
                sendInstructions(instructions);
            }
        };

        class SpeedAdaptTestRequest3 final : public AnimationRequest
        {
        public:
            virtual void finalize() override final
            {
                Instructions instructions;
                // lower turn speed so we can actually spot it
                instructions.turn_speed = 8;
                instructions.turn_speed_steps = 5;
                instructions.set_detect_speed_change(20 * 2 + 0, true);
                instructions.set_detect_speed_change(20 * 2 + 1, true);

                int speed = 32;
                for (int handle_id = 0; handle_id < 1; ++handle_id)
                {
                    for (int idx = 0; idx < 20; ++idx)
                    {
                        instructions.add(20 * 2 + handle_id, DeflatedCmdKey(ANTI_CLOCKWISE | RELATIVE, 90, speed));
                        instructions.add(22 * 2 + handle_id, DeflatedCmdKey(CLOCKWISE | RELATIVE, 90, speed));
                        instructions.add(20 * 2 + handle_id, DeflatedCmdKey(CLOCKWISE | RELATIVE, 90, speed));
                        instructions.add(22 * 2 + handle_id, DeflatedCmdKey(ANTI_CLOCKWISE | RELATIVE, 90, speed));
                        //instructions.add(20 * 2 + handle_id, DeflatedCmdKey(CLOCKWISE | RELATIVE | GHOST, 60, speed));
                        //instructions.add(20 * 2 + handle_id, DeflatedCmdKey(CLOCKWISE | RELATIVE, 60, speed));
                        //instructions.add(20 * 2 + handle_id, DeflatedCmdKey(CLOCKWISE | RELATIVE | GHOST, 60, speed));
                    }
                    instructions.add(20 * 2 + handle_id, DeflatedCmdKey(ANTI_CLOCKWISE | RELATIVE, 360, speed));
                    instructions.add(22 * 2 + handle_id, DeflatedCmdKey(CLOCKWISE | RELATIVE, 360, speed));
                }
                sendInstructions(instructions);
            }
        };

        class SpeedAdaptTestRequest2 final : public AnimationRequest
        {
        public:
            virtual void finalize() override final
            {
                Instructions instructions;
                instructions.set_detect_speed_change(20 * 2 + 0, true);
                instructions.set_detect_speed_change(20 * 2 + 1, false);
                instructions.set_detect_speed_change(2 * 2 + 0, true);
                instructions.set_detect_speed_change(2 * 2 + 1, true);

                int speed = 8;
                for (int idx = 0; idx < 4; ++idx)
                {
                    instructions.add(20 * 2 + 0, DeflatedCmdKey(CLOCKWISE | RELATIVE, 90, speed));
                    instructions.add(20 * 2 + 0, DeflatedCmdKey(CLOCKWISE | RELATIVE | GHOST, 90, speed));
                    instructions.add(20 * 2 + 0, DeflatedCmdKey(CLOCKWISE | RELATIVE, 90, speed));
                    instructions.add(20 * 2 + 0, DeflatedCmdKey(CLOCKWISE | RELATIVE | GHOST, 90, speed));
                    instructions.add(20 * 2 + 0, DeflatedCmdKey(CLOCKWISE | RELATIVE, 90, speed));
                    instructions.add(20 * 2 + 0, DeflatedCmdKey(CLOCKWISE | RELATIVE | GHOST, 90, speed));

                    instructions.add(20 * 2 + 1, DeflatedCmdKey(CLOCKWISE | RELATIVE, 90, speed / 2));
                    instructions.add(20 * 2 + 1, DeflatedCmdKey(CLOCKWISE | RELATIVE, 90, speed / 2));
                    instructions.add(20 * 2 + 1, DeflatedCmdKey(CLOCKWISE | RELATIVE, 90, speed / 2));

                    /*
                    instructions.swap_speed_detection = true;
                    instructions.add(20 * 2 + 0, DeflatedCmdKey(CLOCKWISE | CmdEnum::RELATIVE, 360, speed));
                    instructions.add(20 * 2 + 0, DeflatedCmdKey(ANTI_CLOCKWISE | CmdEnum::RELATIVE, 360, speed));
                    instructions.add(20 * 2 + 0, DeflatedCmdKey(GHOST | CLOCKWISE | CmdEnum::RELATIVE, 360, speed));
                    instructions.add(20 * 2 + 0, DeflatedCmdKey(GHOST | ANTI_CLOCKWISE | CmdEnum::RELATIVE, 360, speed));
                    instructions.add(20 * 2 + 0, DeflatedCmdKey(CLOCKWISE | CmdEnum::RELATIVE, 360, speed));
                    instructions.add(20 * 2 + 0, DeflatedCmdKey(ANTI_CLOCKWISE | CmdEnum::RELATIVE, 360, speed));
                    

                    instructions.swap_speed_detection = false;
                    instructions.add(20 * 2 + 1, DeflatedCmdKey(CLOCKWISE | CmdEnum::RELATIVE, 360, speed));
                    instructions.add(20 * 2 + 1, DeflatedCmdKey(ANTI_CLOCKWISE | CmdEnum::RELATIVE, 360, speed));
                    instructions.add(20 * 2 + 1, DeflatedCmdKey(CLOCKWISE | CmdEnum::RELATIVE, 360, speed));
                    instructions.add(20 * 2 + 1, DeflatedCmdKey(ANTI_CLOCKWISE | CmdEnum::RELATIVE, 360, speed));
                    instructions.add(20 * 2 + 1, DeflatedCmdKey(CLOCKWISE | CmdEnum::RELATIVE, 360, speed));
                    instructions.add(20 * 2 + 1, DeflatedCmdKey(ANTI_CLOCKWISE | CmdEnum::RELATIVE, 360, speed));
                    */
                    // instructions.add(20 * 2 + 1, DeflatedCmdKey(ANTI_CLOCKWISE | CmdEnum::RELATIVE, 720, 16));

                    //instructions.swap_speed_detection = false;
                    //instructions.add(22 * 2 + 0, DeflatedCmdKey(CLOCKWISE | CmdEnum::RELATIVE, 360, 16));
                    //instructions.add(22 * 2 + 0, DeflatedCmdKey(ANTI_CLOCKWISE | CmdEnum::RELATIVE, 360, 16));

                    //instructions.add(22 * 2 + 1, DeflatedCmdKey(ANTI_CLOCKWISE | CmdEnum::RELATIVE, 720, 16));
                }

                sendInstructions(instructions);

                return;
                // instructions.add(22 * 2 + 1, DeflatedCmdKey(CLOCKWISE | CmdEnum::RELATIVE, 720, 8));
                for (int idx = 0; idx < 2; ++idx)
                {
                    instructions.add(22 * 2 + 0, DeflatedCmdKey(CLOCKWISE | CmdEnum::RELATIVE, 90, 4));
                    instructions.add(22 * 2 + 0, DeflatedCmdKey(ANTI_CLOCKWISE | CmdEnum::RELATIVE, 90, 4));
                    instructions.add(22 * 2 + 0, DeflatedCmdKey(CLOCKWISE | CmdEnum::RELATIVE, 90, 32));
                    instructions.add(22 * 2 + 0, DeflatedCmdKey(ANTI_CLOCKWISE | CmdEnum::RELATIVE, 90, 32));
                    instructions.add(22 * 2 + 0, DeflatedCmdKey(CLOCKWISE | CmdEnum::RELATIVE, 90, 16));
                    instructions.add(22 * 2 + 0, DeflatedCmdKey(ANTI_CLOCKWISE | CmdEnum::RELATIVE, 90, 16));
                    instructions.add(22 * 2 + 0, DeflatedCmdKey(CLOCKWISE | CmdEnum::RELATIVE, 90, 8));
                    instructions.add(22 * 2 + 0, DeflatedCmdKey(ANTI_CLOCKWISE | CmdEnum::RELATIVE, 90, 8));
                    instructions.add(22 * 2 + 0, DeflatedCmdKey(CLOCKWISE | CmdEnum::RELATIVE, 90, 16));
                    instructions.add(22 * 2 + 0, DeflatedCmdKey(ANTI_CLOCKWISE | CmdEnum::RELATIVE, 90, 16));
                    instructions.add(22 * 2 + 0, DeflatedCmdKey(CLOCKWISE | CmdEnum::RELATIVE, 90, 8));
                    instructions.add(22 * 2 + 0, DeflatedCmdKey(ANTI_CLOCKWISE | CmdEnum::RELATIVE, 90, 8));
                    instructions.add(22 * 2 + 0, DeflatedCmdKey(CLOCKWISE | CmdEnum::RELATIVE, 90, 32));
                    instructions.add(22 * 2 + 0, DeflatedCmdKey(ANTI_CLOCKWISE | CmdEnum::RELATIVE, 90, 32));
                }

                for (int idx = 0; idx < 2; ++idx)
                {
                    instructions.add(20 * 2 + 0, DeflatedCmdKey(CLOCKWISE | CmdEnum::RELATIVE, 90, 4));
                    instructions.add(20 * 2 + 0, DeflatedCmdKey(ANTI_CLOCKWISE | CmdEnum::RELATIVE, 90, 4));
                    instructions.add(20 * 2 + 0, DeflatedCmdKey(CLOCKWISE | CmdEnum::RELATIVE, 90, 32));
                    instructions.add(20 * 2 + 0, DeflatedCmdKey(ANTI_CLOCKWISE | CmdEnum::RELATIVE, 90, 32));
                    instructions.add(20 * 2 + 0, DeflatedCmdKey(CLOCKWISE | CmdEnum::RELATIVE, 90, 16));
                    instructions.add(20 * 2 + 0, DeflatedCmdKey(ANTI_CLOCKWISE | CmdEnum::RELATIVE, 90, 16));
                    instructions.add(20 * 2 + 0, DeflatedCmdKey(CLOCKWISE | CmdEnum::RELATIVE, 90, 8));
                    instructions.add(20 * 2 + 0, DeflatedCmdKey(ANTI_CLOCKWISE | CmdEnum::RELATIVE, 90, 8));
                    instructions.add(20 * 2 + 0, DeflatedCmdKey(CLOCKWISE | CmdEnum::RELATIVE, 90, 16));
                    instructions.add(20 * 2 + 0, DeflatedCmdKey(ANTI_CLOCKWISE | CmdEnum::RELATIVE, 90, 16));
                    instructions.add(20 * 2 + 0, DeflatedCmdKey(CLOCKWISE | CmdEnum::RELATIVE, 90, 8));
                    instructions.add(20 * 2 + 0, DeflatedCmdKey(ANTI_CLOCKWISE | CmdEnum::RELATIVE, 90, 8));
                    instructions.add(20 * 2 + 0, DeflatedCmdKey(CLOCKWISE | CmdEnum::RELATIVE, 90, 32));
                    instructions.add(20 * 2 + 0, DeflatedCmdKey(ANTI_CLOCKWISE | CmdEnum::RELATIVE, 90, 32));
                }
                sendInstructions(instructions);
            };
        };

        class SpeedTestRequest32 final : public AnimationRequest
        {
        public:
            virtual void finalize() override final
            {

                Instructions instructions;
                instructions.add(22 * 2 + 1, DeflatedCmdKey(CLOCKWISE | CmdEnum::RELATIVE, 720, 4));

                instructions.add(22 * 2 + 0, DeflatedCmdKey(CLOCKWISE | CmdEnum::RELATIVE, 720, 8));
                instructions.add(22 * 2 + 0, DeflatedCmdKey(CLOCKWISE | CmdEnum::RELATIVE, 720, 8));

                instructions.add(20 * 2, DeflatedCmdKey(CLOCKWISE | CmdEnum::RELATIVE, 720, 8));
                instructions.add(20 * 2, DeflatedCmdKey(CLOCKWISE | CmdEnum::RELATIVE, 720, 8));

                for (int idx = 0; idx < 8; ++idx)
                    instructions.add(20 * 2 + 1, DeflatedCmdKey(CLOCKWISE | CmdEnum::RELATIVE, 720, 32));

                sendInstructions(instructions);
            };
        };

        class SpeedTestRequest64 final : public AnimationRequest
        {
        public:
            virtual void finalize() override final
            {
                Instructions instructions;
                instructions.add(20 * 2 + 0, DeflatedCmdKey(CLOCKWISE | CmdEnum::RELATIVE, 720, 4));

                instructions.add(20 * 2 + 1, DeflatedCmdKey(CLOCKWISE | CmdEnum::RELATIVE, 720, 8));
                instructions.add(20 * 2 + 1, DeflatedCmdKey(CLOCKWISE | CmdEnum::RELATIVE, 720, 8));

                instructions.add(22 * 2 + 1, DeflatedCmdKey(ANTI_CLOCKWISE | CmdEnum::RELATIVE, 720, 8));
                instructions.add(22 * 2 + 1, DeflatedCmdKey(ANTI_CLOCKWISE | CmdEnum::RELATIVE, 720, 8));

                for (int idx = 0; idx < 16; ++idx)
                {
                    instructions.add(22 * 2 + 0, DeflatedCmdKey(CLOCKWISE | CmdEnum::RELATIVE, 720, 64));
                }
                sendInstructions(instructions);
            };
        };

        class ZeroPosition final : public AnimationRequest 
        {
            public:
            virtual void finalize() override final
            {
                HandlesState goal;
                goal.setAll(0, 0);

                Instructions instructions;

                sendInstructions(instructions, 1);
            }

        };

        class SixPosition final : public AnimationRequest 
        {
            public:
            virtual void finalize() override final
            {
                HandlesState goal;
                goal.setAll(NUMBER_OF_STEPS/2, NUMBER_OF_STEPS/2);

                Instructions instructions;

                sendInstructions(instructions, 1);
            }

        };

        class TrackTimeRequest final : public AnimationRequest
        {
            const oclock::time_tracker::TimeTracker &tracker;

            DistanceCalculators::Func selectDistanceCalculator()
            {
                auto value = oclock::master.get_handles_distance_mode();
                switch (value)
                {
#define CASE(WHAT, FUNC)            \
    case HandlesDistanceEnum::WHAT: \
        return DistanceCalculators::FUNC;

                    CASE(Shortest, shortest)
                    CASE(Right, clockwise)
                    CASE(Left, antiClockwise)
                default:
                    CASE(Random, random())
#undef CASE
                }
            }

            InBetweenAnimations::Func selectInBetweenAnimation()
            {
                auto value = oclock::master.get_in_between_animation();
                switch (value)
                {
#define CASE(WHAT, FUNC)               \
    case InBetweenAnimationEnum::WHAT: \
        return InBetweenAnimations::FUNC;

                    CASE(Random, instructRandom)
                    CASE(Star, instructStarAnimation)
                    CASE(Dash, instructDashAnimation)
                    CASE(Middle1, instructMiddlePointAnimation)
                    CASE(Middle2, instructAllInnerPointAnimation)
                    CASE(PacMan, instructPacManAnimation)
                default:
                    CASE(None, instructNone)
#undef CASE
                }
            }

            HandlesAnimations::Func selectFinalAnimator()
            {
                auto value = oclock::master.get_handles_animation_mode();
                switch (value)
                {
#define CASE(WHAT, FUNC)             \
    case HandlesAnimationEnum::WHAT: \
        return HandlesAnimations::FUNC;

                    CASE(Swipe, instruct_using_swipe)
                    CASE(Distance, instructUsingStepCalculator)
                default:
                    CASE(Random, instruct_using_random)
#undef CASE
                }
            }

        public:
            TrackTimeRequest(const oclock::time_tracker::TimeTracker &tracker) : tracker(tracker) {}

            virtual void finalize() override final
            {
                ESP_LOGI(TAG, "do_track_time -> follow up");

                auto now = tracker.now();
                auto hour = now.hour;
                auto minute = now.minute;

                // get characters
                auto clockChars = ClockUtil::retrieveClockCharactersfromDigitalTime(hour, minute);

                HandlesState goal;
                copyTo(clockChars, goal);
                bool act_as_second_handle = true;

                Instructions instructions;
                if (act_as_second_handle)
                    instructions.iterate_handle_ids(
                        [&](int handle_id)
                        {
                            if (!goal.visibilityFlags[handle_id])
                                // oke move to 12:00
                                goal.set_ticks(handle_id, 0);
                        });
                

                // final animation

                // inbetween
                auto inBetweenAnimation = selectInBetweenAnimation();
                auto distanceCalculator = selectDistanceCalculator();
                auto finalAnimator = selectFinalAnimator();

                auto speed = tracker.get_speed_multiplier() * oclock::master.get_base_speed();

                selectInBetweenAnimation()(instructions, speed);
                selectFinalAnimator()(instructions, speed, goal, distanceCalculator);

                // lets wait for all... 
                InBetweenAnimations::instructDelayUntilAllAreReady(instructions, 32);
                float millis_left = 1000.0f * (60.0f - tracker.now().second) / float(tracker.get_speed_multiplier());
                ESP_LOGI(TAG, "millis_left: %f", millis_left);
                if (act_as_second_handle)
                    instructions.iterate_handle_ids(
                        [&](int handle_id)
                        {
                            if (!goal.visibilityFlags[handle_id])
                                instructions.follow_seconds(handle_id, true);
                        });

                sendInstructions(instructions, millis_left);
            }
        };

        class PositionRequest final : public oclock::BroadcastRequest
        {
            const bool stop_;

        public:
            PositionRequest(bool stop) : stop_(stop) {}

            virtual void execute() override final
            {
                animationController.reset_handles();
                send(UartPosRequest(stop_));
            }
        };

        class BackgroundModeSelectRequest final : public oclock::ExecuteRequest
        {
        public:
            BackgroundModeSelectRequest(int mode)
            {
                oclock::master.set_led_background_mode(mode);
            }

            virtual void execute() override final
            {
                // just send latests
                auto mode = oclock::master.get_led_background_mode();
                send(LedModeRequest(false, mode));
            }
        };

        class ForegroundModeSelectRequest final : public oclock::ExecuteRequest
        {
        public:
            ForegroundModeSelectRequest(int mode)
            {
                oclock::master.set_led_foreground_mode(mode);
            }

            virtual void execute() override final
            {
                // just send latests
                auto mode = oclock::master.get_led_foreground_mode();
                send(LedModeRequest(true, mode));
            }
        };

        class DumpSlaveLogsRequest : public BroadcastRequest
        {
            const bool also_config_;

        public:
            DumpSlaveLogsRequest(bool also_config) : also_config_(also_config) {}
            virtual void execute() override final
            {
                send(UartDumpLogsRequest(also_config_));
            }
        };
    }
}