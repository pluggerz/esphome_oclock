#ifdef ESP8266

#include "requests.h"

// UartColorMessage uartColorMessage;
//  UartColorMessage sendUartColorMessage;

class RgbLedColorRequest final : public oclock::ExecuteRequest
{
    oclock::RgbColorLeds leds;

    virtual void execute() override
    {
        send(UartRgbLedsMessage(leds));
    }

public:
    RgbLedColorRequest(const oclock::RgbColorLeds &leds) : ExecuteRequest("RgbLedColorRequest"), leds(leds) {}
};

void oclock::requests::publish_rgb_leds(const oclock::RgbColorLeds &leds)
{
    oclock::queue(new RgbLedColorRequest(leds));
};

bool ledColorRequestIsQueued = false;
class LedColorRequest final : public oclock::ExecuteRequest
{
    virtual void execute() override
    {
        auto color = oclock::master.get_background_color();
        ESP_LOGI(TAG, "send: (r,g,b)=(%02x, %02x, %02x)", color.red, color.green, color.blue);
        send(UartColorMessage(color));
        ledColorRequestIsQueued = false;
    }

public:
    LedColorRequest(const oclock::RgbColor &color) : ExecuteRequest("LedColorRequest")
    {
        ledColorRequestIsQueued = true;
    }
};

class SetScaledBrightnessRequest final : public oclock::ExecuteRequest
{
public:
    SetScaledBrightnessRequest(int brightness) : oclock::ExecuteRequest("SetScaledBrightnessRequest")
    {
        // we immediatelly update the brightness, in case we increase it again before sending to the slaves
        if (brightness < 0)
            brightness = 0;
        else if (brightness > 5)
            brightness = 5;
        else
            brightness = brightness;
        oclock::master.set_brightness(brightness);
    }

    virtual void execute() override final
    {
        // just send latests
        send(UartScaledBrightnessMessage(oclock::master.get_brightness()));
    }
};

void oclock::requests::publish_brightness(const int value)
{
    if (value == oclock::master.get_brightness())
        return;
    oclock::queue(new SetScaledBrightnessRequest(value));
}
void oclock::requests::publish_background_color(const RgbColor &color)
{
    oclock::master.set_background_color(color);
    if (!ledColorRequestIsQueued)
        oclock::queue(new LedColorRequest(color));
}

void oclock::requests::publish_background_color(const int component_, const float value_)
{
    auto color = oclock::master.get_background_color();
    uint8_t value = value_ * 0xFF;
    switch (component_)
    {
#define CASE(COMPONENT, FIELD) \
    case COMPONENT:            \
        color.FIELD = value;   \
        break;

        CASE(0, red);
        CASE(1, green);
        CASE(2, blue);
    }
    publish_background_color(color);
}

#endif