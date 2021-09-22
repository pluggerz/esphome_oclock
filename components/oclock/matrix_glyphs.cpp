#include "matrix_glyphs.h"

#define TAG "matrix_glyphs"

using esphome::switch_::Switch;
using std::shared_ptr;
using std::unique_ptr;

namespace esphome
{
    namespace matrix_glyphs
    {
        Controller controller;

        class StickySwitch : public Switch
        {
        public:
            virtual void write_state(bool state) override
            {
                ESP_LOGI(TAG, "StickySwitch::write_state %s", state ? "true" : "false");
                publish_state(state);
            }
        };

        BinarySensorWidget::BinarySensorWidget() : sticky_switch_(new StickySwitch())
        {
        }
    }
}

esphome::matrix_glyphs::AnimationGlyph::AnimationGlyph(const std::vector<std::string> &mdiNames)
{
    for (auto it = std::begin(mdiNames); it != std::end(mdiNames); ++it)
    {
        glyphs_.push_back(std::make_shared<MdiGlyph>(*it));
    }
}

void esphome::matrix_glyphs::SensorWidget::set_sensor(Sensor *source)
{
    source_ = source;
    if (source_->get_unit_of_measurement() == "°C" || source_->get_device_class() == "temperature")
    {
        icon = std::make_shared<MdiGlyph>("thermometer");
    }
}

void esphome::matrix_glyphs::BinarySensorWidget::set_sensor(BinarySensor *source)
{
    _source = source;
    _source->add_on_state_callback([this](bool state)
                                   { this->state_callback(state); });

    if (!on_glyph_ && !off_glyph_)
    {
        const auto &device_class = _source->get_device_class();
        if (device_class == "motion")
        {
            on_glyph_ = std::make_shared<AnimationGlyph>(AnimationGlyph({"run", "walk"}));
        }
        else
        {
            on_glyph_ = std::make_shared<MdiGlyph>("keyboard-space");
            off_glyph_ = std::make_shared<MdiGlyph>("keyboard-space");
        }
    }
}
