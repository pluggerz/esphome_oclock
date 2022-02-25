#ifdef ESP8266

#include "requests.h"

// UartColorMessage uartColorMessage;
//  UartColorMessage sendUartColorMessage;

class RgbBackgroundLedsRequest final : public oclock::ExecuteRequest
{
    oclock::RgbColorLeds leds;

    virtual void execute() override
    {
        send(UartRgbBackgroundLedsMessage(leds));
    }

public:
    RgbBackgroundLedsRequest(const oclock::RgbColorLeds &leds) : ExecuteRequest("RgbBackgroundLedsRequest"), leds(leds) {}
};

class RgbForegroundLedsRequest final : public oclock::ExecuteRequest
{
    oclock::RgbColorLeds leds;

    virtual void execute() override
    {
        send(UartRgbForegroundLedsMessage(leds));
    }

public:
    RgbForegroundLedsRequest(const oclock::RgbColorLeds &leds) : ExecuteRequest("RgbForegroundLedsRequest"), leds(leds) {}
};

void oclock::requests::publish_background_rgb_leds(const oclock::RgbColorLeds &leds)
{
    oclock::queue(new RgbBackgroundLedsRequest(leds));
};

void oclock::requests::publish_foreground_rgb_leds(const oclock::RgbColorLeds &leds)
{
    oclock::queue(new RgbForegroundLedsRequest(leds));
};

bool ledColorRequestIsQueued = false;
class LedColorRequest final : public oclock::ExecuteRequest
{
    virtual void execute() override
    {
        int h = oclock::master.get_background_color_h();
        auto color = oclock::RgbColor::h_to_rgb(h);
        ESP_LOGI(TAG, "send: [h](r,g,b)=[%d](%f, %f, %f)", h, color.get_red() / 255.0, color.get_green() / 255.0, color.get_blue() / 255.0);
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

void oclock::requests::publish_background_color_h(int h)
{
    auto color = oclock::RgbColor::h_to_rgb(h);
    oclock::master.set_background_color_h(h);
    if (!ledColorRequestIsQueued)
        oclock::queue(new LedColorRequest(color));
}

#endif