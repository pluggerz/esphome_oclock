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
                            const auto ghosting = cmd.ghost();
                            const auto steps = cmd.steps();
                            const auto speed = cmdSpeedUtil.deflate_speed(cmd.inflated_speed());
                            const auto clockwise = cmd.clockwise();
                            const auto relativePosition = true;
                            ESP_LOGI(TAG, "Cmd: %s=%3d sp=%d gh=%s cl=%s", ghosting || relativePosition ? "steps" : "   to", steps, speed, YESNO(ghosting), ghosting ? "N/A" : YESNO(clockwise));
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
                    // if (hours == 13)
                    //    state.visibilityFlags.hide(handleId);
                };
                ClockUtil::iterateHandles(chars, lambda);
                for (int handleId = 0; handleId < MAX_HANDLES; handleId += 2)
                {
                    //if (state[handleId] == state[handleId + 1])
                    //    state.nonOverlappingFlags.hide(handleId);
                }
                // state.debug();
            }

            void updateSpeeds(const Instructions &instructions)
            {
                // gather speeds
                std::set<int> new_speeds_as_set{1};
                for (const auto &cmd : instructions.cmds)
                {
                    if (cmd.ignorable())
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

            void sendInstructions(Instructions &instructions)
            {
                updateSpeeds(instructions);

                // lets start transmitting
                send(UartMessage(-1, MsgType::MSG_BEGIN_KEYS));

                // send instructions
                sendCommands(instructions.cmds);
                instructions.dump();
                // finalize
                u16 millisLeft = 1; //(60 - t.seconds) * 1000 + (1000 - t.millis);
                send(UartEndKeysMessage(
                    instructions.turn_speed,
                    instructions.turn_speed_steps,
                    cmdSpeedUtil.get_speeds(),
                    instructions.get_speed_detection(),
                    millisLeft));
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

        class TrackTimeRequest final : public AnimationRequest
        {
            const oclock::time_tracker::TimeTracker &tracker;

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

                Instructions instructions;

                StepCalculator *calculator;
                auto speed=tracker.get_speed_multiplier() * 12;
                switch (random(3))
                {
                case 0:
                    calculator = &shortestPathCalculator;
                    ESP_LOGI(TAG, "Using shortestPathCalculator");
                    break;
                case 1:
                    calculator = &clockwiseCalculator;
                    ESP_LOGI(TAG, "Using clockwiseCalculator");
                    break;
                case 2:
                default:
                    calculator = &antiClockwiseCalculator;
                    ESP_LOGI(TAG, "Using antiClockwiseCalculator");
                    break;
                }
                switch (random(7))
                {
                case 0:
                    instructUsingStepCalculator(instructions, speed, goal, *calculator);
                    ESP_LOGI(TAG, "Using instructUsingStepCalculator");
                    break;

                case 1:
                    instructUsingSwipe(instructions, speed, goal, *calculator);
                    ESP_LOGI(TAG, "Using instructUsingSwipe");
                    break;

                case 2:
                    InBetweenAnimations::instructPacManAnimation(instructions, speed);
                    instructUsingStepCalculator(instructions, speed, goal, *calculator);
                    ESP_LOGI(TAG, "Using instructPacManAnimation");
                    break;

                case 3:
                    InBetweenAnimations::instructAllInnerPointAnimation(instructions, speed);
                    instructUsingStepCalculator(instructions, speed, goal, *calculator);
                    ESP_LOGI(TAG, "Using instructAllInnerPointAnimation");
                    break;

                case 4:
                    InBetweenAnimations::instructMiddlePointAnimation(instructions, speed);
                    instructUsingStepCalculator(instructions, speed, goal, *calculator);
                    ESP_LOGI(TAG, "Using instructMiddlePointAnimation");
                    break;

                case 5:
                    InBetweenAnimations::instructDashAnimation(instructions, speed);
                    instructUsingStepCalculator(instructions, speed, goal, *calculator);
                    ESP_LOGI(TAG, "Using instructDashAnimation");
                    break;

                case 6:
                default:
                    InBetweenAnimations::instructStarAnimation(instructions, speed);
                    instructUsingStepCalculator(instructions, speed, goal, *calculator);
                    ESP_LOGI(TAG, "Using instructStarAnimation");
                    break;
                }
                sendInstructions(instructions);
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

        class SwitchLedModeRequest final : public oclock::ExecuteRequest
        {
        public:
            SwitchLedModeRequest()
            {
                // we immediatelly update the brigness, incase we increase it again before sending to the slaves
                auto current = oclock::master.get_led_background_mode();
                oclock::master.set_led_background_mode(current + 1);
            }

            virtual void execute() override final
            {
                // just send latests
                auto brightness = oclock::master.get_brightness();
                auto mode = oclock::master.get_led_background_mode();
                send(LedModeRequest(mode, brightness));
            }
        };

        class SetBrightnessRequest final : public oclock::ExecuteRequest
        {
        public:
            SetBrightnessRequest(int brightness)
            {
                // we immediatelly update the brigness, incase we increase it again before sending to the slaves
                if (brightness < 0)
                    brightness = 0;
                else if (brightness > 31)
                    brightness = 31;
                else
                    brightness = brightness;
                oclock::master.set_brightness(brightness);
            }

            virtual void execute() override final
            {
                // just send latests
                auto brightness = master.get_brightness();
                auto mode = master.get_led_background_mode();
                send(LedModeRequest(mode, brightness));
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