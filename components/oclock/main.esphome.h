
#include "master.h"
#include "requests.h"

#include "esphome/components/switch/switch.h"

namespace oclock
{
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

    class LedModeConfigSwitch : public esphome::switch_::Switch
    {
    public:
        virtual void write_state(bool state) override
        {
            publish_state(false);
            oclock::queue(new oclock::requests::SwitchLedModeRequest());
        }
    };
}