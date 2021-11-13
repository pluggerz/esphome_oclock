#ifdef ESP8266

#include "requests.h"

UartColorMessage uartColorMessage;
UartColorMessage sendUartColorMessage;

class LedColorRequest final : public oclock::ExecuteRequest
{
    virtual void execute() override
    {
        if (!memcmp(&uartColorMessage, &sendUartColorMessage, sizeof(UartColorMessage)))
        {
            ESP_LOGD(TAG, "ignore send: (r,g,b)=(%02x, %02x, %02x)",
                     uartColorMessage.red, uartColorMessage.green, uartColorMessage.blue);
            return;
        }
        ESP_LOGI(TAG, "send: (r,g,b)=(%02x, %02x, %02x)",
                 uartColorMessage.red, uartColorMessage.green, uartColorMessage.blue);
        sendUartColorMessage = uartColorMessage;
        send(sendUartColorMessage);
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
        send(UartBrightnessMessage(oclock::master.get_brightness()));
    }
};

void oclock::requests::background_brightness(const float value_)
{
    uint8_t value = value_ * 31;
    if (value == oclock::master.get_brightness())
        return;
    oclock::master.set_brightness(value);

    oclock::queue(new SetBrightnessRequest(value));
}

void oclock::requests::background_color(const int component_, const float value_)
{
    uint8_t value = value_ * 0xFF;
    bool do_send = false;
    switch (component_)
    {
#define CASE(COMPONENT, FIELD)               \
    case COMPONENT:                          \
        if (uartColorMessage.FIELD == value) \
            do_send = true;                  \
        uartColorMessage.FIELD = value;      \
        break;

        CASE(0, red);
        CASE(1, green);
        CASE(2, blue);
    }
    uartColorMessage.brightness = 31;
    if (!do_send)
    {
        ESP_LOGD(TAG, "ignoring: color=%d value=%f", component_, value_);
        return;
    }

    oclock::queue(new LedColorRequest());
}

#endif