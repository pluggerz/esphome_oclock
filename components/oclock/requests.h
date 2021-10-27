#pragma once

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
            void sendCommandsForHandle(int animatorHandleId, const std::vector<Cmd> &commands)
            {
                auto physicalHandleId = animationController.mapAnimatorHandle2PhysicalHandleId(animatorHandleId);
                if (physicalHandleId < 0 || commands.empty())
                    // ignore
                    return;

                auto nmbrOfCommands = commands.size();
                UartKeysMessage msg(physicalHandleId, (u8)nmbrOfCommands);
                for (std::size_t idx = 0; idx < nmbrOfCommands; ++idx)
                {
                    msg.setCmd(idx, commands[idx]);
                }

                if (true)
                {
                    ESP_LOGI(TAG, "send(A%02d, animatorHandleId=%d->physicalHandleId=%d size: %d",
                             animatorHandleId >> 1, animatorHandleId, physicalHandleId, commands.size());
                    for (std::size_t idx = 0; idx < nmbrOfCommands; ++idx)
                    {
                        const auto &cmd = msg.getCmd(idx);
                        const auto ghosting = (cmd.mode & CmdEnum::GHOST) != 0;
                        const auto steps = cmd.steps;
                        const auto speed = cmd.speed;
                        const auto clockwise = (cmd.mode & CmdEnum::CLOCKWISE) != 0;
                        const auto relativePosition = (cmd.mode & CmdEnum::ABSOLUTE) == 0;
                        ESP_LOGI(TAG, "Cmd: %s=%3d speed=%d ghosting=%s clockwise=%s (%f)", ghosting || relativePosition ? "steps" : "   to", steps, speed, YESNO(ghosting), ghosting ? "N/A" : YESNO(clockwise), cmd.time());
                    }

                    ESP_LOGI(TAG, "  done");
                }
                send(msg);
            }

            void sendAndClear(int handleId, std::vector<Cmd> &selected)
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
                std::vector<Cmd> selected;
                int lastHandleId = -1;
                for (auto it = std::begin(cmds); it != std::end(cmds); ++it)
                {
                    const auto &handleCmd = *it;
                    if (handleCmd.handleId == -1)
                    {
                        // ignore ('deleted')
                        continue;
                    }
                    if (handleCmd.handleId != lastHandleId)
                    {
                        sendAndClear(lastHandleId, selected);
                        lastHandleId = handleCmd.handleId;
                    }
                    selected.push_back(handleCmd.cmd);
                    if (selected.size() == MAX_ANIMATION_KEYS_PER_MESSAGE)
                    {
                        sendAndClear(lastHandleId, selected);
                    }
                }
                sendAndClear(lastHandleId, selected);
                cmds.clear();
            }

            static void copyTo(const ClockCharacters &chars, HandlesState &state)
            {
                auto lambda = [&state](int handleId, int hours)
                {
                    int goal = static_cast<int>(NUMBER_OF_STEPS * static_cast<double>(hours == 13 ? 7.5 : (double)hours) / 12.0);
                    state[handleId] = goal;
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

            void sendInstructions(Instructions &instructions)
            {
                // lets start transmitting
                send(UartMessage(-1, MsgType::MSG_BEGIN_KEYS));

                // send instructions
                sendCommands(instructions.cmds);
                instructions.dump();
                // finalize
                u16 millisLeft = 1; //(60 - t.seconds) * 1000 + (1000 - t.millis);
                send(UartEndKeysMessage(cmdSpeedUtil.get_speeds(), millisLeft));
            }

        public:
            virtual void execute() override final
            {
                animationController.reset_handles();
                send(UartPosRequest(false));
            }
        };

        class SpeedTestRequest32 final : public AnimationRequest
        {
        public:
            virtual void finalize() override final
            {
                cmdSpeedUtil.set_speeds({1, 2, 4, 8, 16, 32, 64, 128});
                for (int speed = 1; speed < 128; ++speed)
                {
                    auto inflated_speed = cmdSpeedUtil.inflate_speed(speed);
                    auto deflated_speed = cmdSpeedUtil.deflate_speed(inflated_speed);
                    ESP_LOGI(TAG, "speed=%d -> inflated_speed=%d -> deflated_speed=%d", speed, inflated_speed, deflated_speed);
                }
                Instructions instructions;
                instructions.add(22 * 2 + 1, Cmd(CLOCKWISE | CmdEnum::RELATIVE, 720, 4));

                instructions.add(22 * 2 + 0, Cmd(CLOCKWISE | CmdEnum::RELATIVE, 720, 8));
                instructions.add(22 * 2 + 0, Cmd(CLOCKWISE | CmdEnum::RELATIVE, 720, 8));

                instructions.add(20 * 2, Cmd(CLOCKWISE | CmdEnum::RELATIVE, 720, 8));
                instructions.add(20 * 2, Cmd(CLOCKWISE | CmdEnum::RELATIVE, 720, 8));

                for (int idx = 0; idx < 8; ++idx)
                    instructions.add(20 * 2 + 1, Cmd(CLOCKWISE | CmdEnum::RELATIVE, 720, 32));

                sendInstructions(instructions);
            };
        };

        class SpeedTestRequest64 final : public AnimationRequest
        {
        public:
            virtual void finalize() override final
            {
                cmdSpeedUtil.set_speeds({1, 2, 4, 8, 16, 32, 64, 128});
                for (int speed = 1; speed < 128; ++speed)
                {
                    auto inflated_speed = cmdSpeedUtil.inflate_speed(speed);
                    auto deflated_speed = cmdSpeedUtil.deflate_speed(inflated_speed);
                    ESP_LOGI(TAG, "speed=%d -> inflated_speed=%d -> deflated_speed=%d", speed, inflated_speed, deflated_speed);
                }
                Instructions instructions;
                instructions.add(22 * 2 + 0, Cmd(CLOCKWISE | CmdEnum::RELATIVE, 720, 4));

                instructions.add(22 * 2 + 1, Cmd(CLOCKWISE | CmdEnum::RELATIVE, 720, 8));
                instructions.add(22 * 2 + 1, Cmd(CLOCKWISE | CmdEnum::RELATIVE, 720, 8));

                instructions.add(20 * 2 + 1, Cmd(CLOCKWISE | CmdEnum::RELATIVE, 720, 8));
                instructions.add(20 * 2 + 1, Cmd(CLOCKWISE | CmdEnum::RELATIVE, 720, 8));

                for (int idx = 0; idx < 16; ++idx)
                    instructions.add(20 * 2 + 0, Cmd(CLOCKWISE | CmdEnum::RELATIVE, 720, 64));

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
                cmdSpeedUtil.set_speeds({2, 4, 8, 12, 18, 22, 28, 32});
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
                
                switch (random(3))
                {
                case 0:
                    calculator = &shortestPathCalculator;
                    break;
                case 1:
                    calculator = &clockwiseCalculator;
                    break;
                case 2:
                default:
                    calculator = &antiClockwiseCalculator;
                    break;
                }
                switch (random(2))
                {
                case 0:
                    instructUsingStepCalculator(instructions, 10, goal, *calculator);
                    break;

                case 1:
                default:
                    instructUsingSwipe(instructions, 10, goal, *calculator);
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