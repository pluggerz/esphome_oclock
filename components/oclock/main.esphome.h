
#include "master.h"
#include "esphome/components/switch/switch.h"

namespace oclock
{
    class ResetSwitch : public esphome::switch_::Switch
    {
    public:
        virtual void write_state(bool state) override
        {
            ESP_LOGI(TAG, "ResetSwitch::write_state %s", state ? "true" : "false");
            publish_state(false);
            oclock::master.change_to_init();
        }
    };
}