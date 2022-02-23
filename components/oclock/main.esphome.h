
#include "master.h"
#include "async.h"
#include "requests.h"

#include "esphome/core/application.h"
#include "esphome/core/preferences.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/template/number/template_number.h"

namespace oclock
{

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

    class NumberControlEx : public template_::TemplateNumber
    {
        virtual void follow_state() = 0;

        virtual void update() override
        {
            TemplateNumber::update();
            ESP_LOGI(TAG, "NumberControl<%s>::update %d", name_.c_str(), int(state));
            follow_state();
        }

        virtual void control(float value) override
        {
            auto rounded_value = int(value);
            TemplateNumber::control(rounded_value);
            ESP_LOGI(TAG, "NumberControl<%s>::control %d", name_.c_str(), int(state));
            follow_state();
        }

    public:
        NumberControlEx()
        {
            set_restore_value(true);
            this->set_update_interval(60000);
            this->set_component_source("template.number");
            App.register_component(this);
            App.register_number(this);
            this->set_disabled_by_default(false);
            this->set_optimistic(true);
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

    class ZeroPositionSwitch : public esphome::switch_::Switch
    {
    public:
        virtual void write_state(bool state) override
        {
            publish_state(false);
            oclock::queue(new requests::WaitUntilAnimationIsDoneRequest());
            oclock::queue(new requests::ZeroPosition(0));
            AsyncRegister::byName("time_tracker", nullptr);
        }
    };

    class SixPositionSwitch : public esphome::switch_::Switch
    {
    public:
        virtual void write_state(bool state) override
        {
            publish_state(false);
            oclock::queue(new requests::WaitUntilAnimationIsDoneRequest());
            oclock::queue(new requests::ZeroPosition(NUMBER_OF_STEPS / 2));
            AsyncRegister::byName("time_tracker", nullptr);
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

    class EspComponents
    {
    public:
        text_sensor::TextSensor *text{nullptr};
        select::Select *active_mode{nullptr};
        select::Select *handles_animation{nullptr};
        select::Select *distance_calculator{nullptr};
        select::Select *inbetween_animation{nullptr};
        number::Number *brightness{nullptr};
        select::Select *background{nullptr};
        select::Select *foreground{nullptr};
        number::Number *speed{nullptr};
        number::Number *turn_speed{nullptr};
        number::Number *turn_steps{nullptr};
        light::LightState *light{nullptr};
    } extern esp_components;
}