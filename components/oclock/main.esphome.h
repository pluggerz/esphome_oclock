
#include "master.h"
#include "requests.h"

#include "esphome/components/output/float_output.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/template/select/template_select.h"

namespace oclock
{
    template <class V>
    class AbstractSelect : public template_::TemplateSelect
    {
    protected:
        std::map<std::string, V> modes;

        void init()
        {
            std::vector<std::string> options;
            for (const auto &entry : modes)
            {
                options.push_back(entry.first);
            }

            this->set_update_interval(60000);
            this->set_component_source("template.select");

            App.register_component(this);
            App.register_select(this);

            this->set_disabled_by_default(false);

            this->traits.set_options(options);

            this->set_optimistic(true);
            this->set_restore_value(true);
        }

        void follow_state()
        {
            auto entry = modes.find(state);
            if (entry != modes.end())
            {
                ESP_LOGI(TAG, "Select: %d (%s)", entry->second, entry->first.c_str());

                execute_state_change(entry->second);
            }
        }

        virtual void execute_state_change(const V &value) = 0;

        virtual void setup() override
        {
            TemplateSelect::setup();
            follow_state();
        }

        virtual void control(const std::string &value) override
        {
            TemplateSelect::control(value);
            follow_state();
        }
    };

    class BackgroundModeSelect : public AbstractSelect<int>
    {
    public:
        BackgroundModeSelect()
        {
            modes["Solid Color"] = 0;
            modes["Warm White Shimmer"] = 1;
            modes["Random Color Walk"] = 2;
            modes["Traditional Colors"] = 3;
            modes["Color Explosion"] = 4;
            modes["Gradient"] = 5;
            modes["Bright Twinkle"] = 6;
            modes["Collision"] = 7;
            modes["Rainbow"] = 8;

            init();

            this->set_initial_option("Rainbow");
            this->set_name("Background Led Animation");
        }

        virtual void execute_state_change(const int &value) override
        {
            oclock::queue(new requests::BackgroundModeSelectRequest(value));
        }
    };

    class HandlesInBetweenAnimationModeSelect : public AbstractSelect<InBetweenAnimationEnum>
    {
    public:
        HandlesInBetweenAnimationModeSelect()
        {
            modes["Random"] = InBetweenAnimationEnum::Random;
            modes["None"] = InBetweenAnimationEnum::None;
            modes["Star"] = InBetweenAnimationEnum::Star;
            modes["Dash"] = InBetweenAnimationEnum::Dash;
            modes["Middle Point 1"] = InBetweenAnimationEnum::Middle1;
            modes["Middle Point 2"] = InBetweenAnimationEnum::Middle1;
            modes["Pac Man"] = InBetweenAnimationEnum::PacMan;

            init();

            this->set_initial_option("Random");
            this->set_name("Handles In Between Animation");
        }

        virtual void execute_state_change(const InBetweenAnimationEnum &value) override
        {
            oclock::master.set_in_between_animation_mode(value);
        }
    };

    class HandlesDistanceModeSelect : public AbstractSelect<HandlesDistanceEnum>
    {
    public:
        HandlesDistanceModeSelect()
        {
            modes["Random"] = HandlesDistanceEnum::Random;
            modes["Left"] = HandlesDistanceEnum::Left;
            modes["Right"] = HandlesDistanceEnum::Right;
            modes["Shortest"] = HandlesDistanceEnum::Shortest;

            init();

            this->set_initial_option("Random");
            this->set_name("Handles Distance Calculator");
        }

        virtual void execute_state_change(const HandlesDistanceEnum &value) override
        {
            oclock::master.set_handles_distance_mode(value);
        }
    };

    class HandlesAnimationModeSelect : public AbstractSelect<HandlesAnimationEnum>
    {
    public:
        HandlesAnimationModeSelect()
        {
            modes["Random"] = HandlesAnimationEnum::Random;
            modes["Swipe"] = HandlesAnimationEnum::Swipe;
            modes["Distance"] = HandlesAnimationEnum::Distance;

            init();

            this->set_initial_option("Random");
            this->set_name("Handles Animation");
        }

        virtual void execute_state_change(const HandlesAnimationEnum &value) override
        {
            oclock::master.set_handles_animation_mode(value);
        }
    };

    class LighFloatOutput : public output::FloatOutput
    {
    public:
        typedef std::function<void(float v)> Listener;
        Listener listener_;

        void write_state(float state) override
        {
            if (listener_)
            {
                listener_(state);
            }
            ESP_LOGI(TAG, "got: state %f", state);
        }
        void set_listener(const Listener &value)
        {
            listener_ = value;
        }
    };

    class NumberControl final : public number::Number, public Component
    {
        typedef std::function<void(int value)> Listener;
        Listener listener_{[](int) {}};

    protected:
        void control(float value) override
        {
            int rounded_value = value;
            ESP_LOGI(TAG, "control: value=%f rounded_value=%d", value, rounded_value);
            this->publish_state(rounded_value);
            this->listener_(rounded_value);
        }

    public:
        void set_listener(const Listener &listener)
        {
            listener_ = listener;
            if (has_state_)
                listener_(state);
        }

        void setup() override
        {
        }
    };

    class ResetSwitch : public esphome::switch_::Switch
    {
    public:
        virtual void write_state(bool state) override
        {
            publish_state(false);
            oclock::master.reset();
        }
    };

    class TrackTestTime : public esphome::switch_::Switch
    {
    public:
        virtual void write_state(bool state) override
        {
            publish_state(false);
            oclock::queue(new requests::TrackTimeRequest(oclock::time_tracker::testTimeTracker));
            AsyncRegister::byName("time_tracker", nullptr);
        }
    };

    class TrackTimeTask final : public AsyncDelay
    {
        const oclock::time_tracker::TimeTracker &tracker;
        int last_minute{tracker.now().minute};

        virtual void setup(Micros _) override
        {
            AsyncDelay::setup(_);

            auto now = tracker.now();
            ESP_LOGI(TAG, "Now is the time %02d:%02d (%S) note: last_minute=%d", now.hour, now.minute, tracker.name(), last_minute);
        }

        virtual void step() override
        {
            auto now = tracker.now();
            int minute = now.minute;
            if (minute == last_minute)
            {
                return;
            }
            last_minute = minute;
            ESP_LOGI(TAG, "Lets animate for time %02d:%02d (%S) note: last_minute=%d", now.hour, now.minute, tracker.name(), last_minute);
            oclock::queue(new requests::TrackTimeRequest(tracker));
        };

    public:
        TrackTimeTask(const oclock::time_tracker::TimeTracker &tracker) : AsyncDelay(100), tracker(tracker) {}
    };

    class TrackHassioTime : public esphome::switch_::Switch
    {
    public:
        virtual void write_state(bool state) override
        {
            publish_state(false);
            AsyncRegister::byName("time_tracker", new TrackTimeTask(oclock::time_tracker::realTimeTracker));
        }
    };

    class TrackInternalTime : public esphome::switch_::Switch
    {
    public:
        virtual void write_state(bool state) override
        {
            publish_state(false);
            AsyncRegister::byName("time_tracker", new TrackTimeTask(oclock::time_tracker::internalClockTimeTracker));
        }
    };

    class SpeedAdaptTestSwitch : public esphome::switch_::Switch
    {
    public:
        virtual void write_state(bool state) override
        {
            publish_state(false);
            oclock::queue(new requests::SpeedAdaptTestRequest());
        }
    };

    class SpeedCheckSwitch32 : public esphome::switch_::Switch
    {
    public:
        virtual void write_state(bool state) override
        {
            publish_state(false);
            oclock::queue(new requests::SpeedTestRequest32());
        }
    };

    class SpeedCheckSwitch64 : public esphome::switch_::Switch
    {
    public:
        virtual void write_state(bool state) override
        {
            publish_state(false);
            oclock::queue(new requests::SpeedTestRequest64());
        }
    };

    class DumpConfigSwitch : public esphome::switch_::Switch
    {
    public:
        virtual void write_state(bool state) override
        {
            publish_state(false);
            oclock::master.dump_config();
        }
    };

    class DumpConfigSlavesSwitch : public esphome::switch_::Switch
    {
    public:
        virtual void write_state(bool state) override
        {
            publish_state(false);
            oclock::queue(new requests::DumpSlaveLogsRequest(true));
        }
    };

    class RequestPositions : public esphome::switch_::Switch
    {
    public:
        virtual void write_state(bool state) override
        {
            publish_state(false);
            oclock::queue(new requests::PositionRequest(false));
        }
    };

    class DumpLogsSwitch : public esphome::switch_::Switch
    {
    public:
        virtual void write_state(bool state) override
        {
            publish_state(false);
            oclock::queue(new oclock::requests::DumpSlaveLogsRequest(false));
        }
    };
}