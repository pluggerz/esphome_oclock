#include "oclock.h"

#ifdef MASTER_MODE

#include "master.h"
#include "uart.h"
#include "rs485.h"
#include "interop.h"

#include "pins.h"

oclock::Master oclock::master;
bool scanForError = false;
int slaveIdCounter = -2;

InteropRS485 uart(RS485_DE_PIN, RS485_RE_PIN);

void oclock::Master::setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(SYNC_IN_PIN, INPUT);
    pinMode(SYNC_OUT_PIN, OUTPUT);

    // what to do with the following pins ?
    // ?? const int MASTER_GPI0_PIN  = 0; // GPI00
    pinMode(MASTER_GPI0_PIN, INPUT);
    pinMode(ESP_TXD_PIN, OUTPUT);
    pinMode(ESP_RXD_PIN, INPUT);
    pinMode(I2C_SDA_PIN, INPUT);
    pinMode(I2C_SCL_PIN, INPUT);

    pinMode(GPIO_14, INPUT);
    pinMode(USB_POWER_PIN, INPUT);

    uart.setup();
    uart.setOwnerId(0xFF);

    // we ignore errors
    scanForError = false;
    // reset slaveIdCounter (every clock takes 2 ids so that is why it is -2)
    slaveIdCounter = -2;

    ESP_LOGI(TAG, "setup -> scanForError=false, slaveIdCounter=-2");
    ESP_LOGI(TAG, "setup -> Sync::read() = %s", Sync::read() ? "HIGH" : "LOW");

    // force a reset, set line to high ( error mode )
    // so everyone will do the same
    Sync::write(HIGH);

    change_to_init();
}

void oclock::Master::loop()
{
    if (loopFunc_)
    {
        loopFunc_(millis());
    }
}

void oclock::Master::change_to_init()
{
    ESP_LOGI(TAG, "change_to_init");

    // we ignore errors
    scanForError = false;
    // reset slaveIdCounter (every clock takes 2 ids so that is why it is -2)
    slaveIdCounter = -2;

    ESP_LOGI(TAG, "change_to_init -> scanForError=false, slaveIdCounter=-2");
    ESP_LOGI(TAG, "change_to_init -> Sync::read() = %s", Sync::read() ? "HIGH" : "LOW");

    // force a reset, set line to high ( error mode )
    // so everyone will do the same
    Sync::write(HIGH);

    // make sure there is enough time for everyone to act
    Sync::sleep(100);

    while (!Sync::read())
    {
        // wait while slave is high
        delay(200);
    }

    // all slaves will stop their work and set sync to high
    uart.send(UartMessage(-1, MsgType::MSG_ID_RESET));

    // lets start
    uart.send(UartMessage(-1, MsgType::MSG_ID_START));

    // some time so everyone can go to low
    delay(200);    
    uart.send(UartAcceptMessage(-1, 0));
    uart.startReceiving();

    change_to_accepting();
}

/***
 * 
 * While accepting we expect only to receive MSG_ID_ACCEPT 
 */
void oclock::Master::change_to_accepting()
{
    ESP_LOGI(TAG, "change_to_accepting");
    // uart
    auto listener = [this](const UartMessage *msg)
    {
        bool sync = Sync::read();
        if (msg->getMessageType() != MsgType::MSG_ID_ACCEPT)
            return false;

        if (sync)
        {
            slaveIdCounter = ((UartAcceptMessage *)msg)->getAssignedId();
            ESP_LOGI(TAG, "Done waiting for all slaves: sync=HIGH and slaveIdCounter=%d", slaveIdCounter);

            change_to_accepted();
        }
        else
        {
            ESP_LOGI(TAG, "Still waiting for some slaves: sync=LOW");
        }
        return true;
    };
    uart.set_listener(listener);

    // our loop func
    loopFunc_ = [](Millis t)
    {
        uart.loop();
        yield();
    };
}

void oclock::Master::change_to_accepted()
{
    ESP_LOGI(TAG, "change_to_accepted: Sync::read()=%s", Sync::read() ? "HIGH" : "LOW");
    Sync::write(LOW);
    uart.send(UartMessage(-1, MSG_ID_DONE));

    while (Sync::read() == HIGH)
    {
        // wait while slave is high
        delay(200);
    }
    // accept errors
    scanForError = true;
    ESP_LOGI(TAG, "change_to_accepted: Sync::read()=%s", Sync::read() ? "HIGH" : "LOW");

    auto listener = [](const UartMessage *msg)
    { return false; };
    uart.set_listener(listener);

    // our loop func
    loopFunc_ = [](Millis t)
    {
        uart.loop();
        yield();
    };

    uart.send(UartPosRequest());
    uart.startReceiving();
}

void oclock::Master::dump_config()
{
    ESP_LOGCONFIG(TAG, "MASTER:");
    ESP_LOGCONFIG(TAG, "  time:");
    if (time_)
    {
        auto now = time_->now();
        char buf[128];
        now.strftime(buf, sizeof(buf), "%c");
        ESP_LOGCONFIG(TAG, "     now: %s", buf);
        ESP_LOGCONFIG(TAG, "     timestamp_now: %d", time_->timestamp_now());
    }
    else
        ESP_LOGCONFIG(TAG, "      N/A");
    uart.dump_config();
}

#ifdef ddcaaffdfdsa

void sendCommandIdReset()
{
    // we ignore errors
    scanForError = false;
    // reset slaveIdCounter (every clock takes 2 ids so that is why it is -2)
    slaveIdCounter = -2;

    ESP_LOGI(TAG, "sendCommandIdReset -> scanForError=false, slaveIdCounter=-2");
    ESP_LOGI(TAG, "sendCommandIdReset -> Sync::read() = %s", Sync::read() ? "HIGH" : "LOW");

    // force a reset, set line to high ( error mode )
    // so everyone will do the same
    Sync::write(HIGH);

    // make sure there is enough time for everyone to act
    Sync::sleep(250);

    // all slaves will stop their work and set sync to high
    ESP_LOGI(TAG, "sendCommandIdReset -> send(UartMessage(-1, MSG_ID_RESET))");
    uart.send(UartMessage(-1, MsgType::MSG_ID_RESET));

    // lets start
    ESP_LOGI(TAG, "sendCommandIdReset -> send(UartMessage(-1, MSG_ID_START))");
    uart.send(UartMessage(-1, MSG_ID_START));
    op_mode_ = OpMode::PRE_ACCEPT;
    ESP_LOGI(TAG, "Change to OpMode::PRE_ACCEPT");
}

/**
 * 
 * Assumption: Sync is high
 */
void sendCommandIdAccept()
{
    uart.send(UartAcceptMessage(-1, 0));
    uart.startReceiving();
}

void sendCommandCycle()
{
    sendCommandIdReset();
    sendCommandIdAccept();
}

void oclock::Master::loop_pre_accept()
{
    auto state = Sync::read();
    ESP_LOGI(TAG, "sendCommandIdReset -> Sync::read() = %s", Sync::read() ? "HIGH" : "LOW");
    if (state)
    {
        op_mode_ = OpMode::ACCEPT;
        ESP_LOGI(TAG, "Change to OpMode::ACCEPT and send UartAcceptMessage");

        uart.send(UartAcceptMessage(-1, 0));
        uart.startReceiving();
    }
}

void oclock::Master::loop()
{
    if (op_mode_ == OpMode::UNDEFINED)
    {
        ESP_LOGI(TAG, "Invalid state!? OpMode::UNDEFINED");
    }
    else if (op_mode_ == OpMode::INITIAL_DELAY)
    {
        auto t = millis();
        if (t < op_delay_t_millis)
            return;

        blink(5);

        // lets start
        sendCommandIdReset();
    }
    else if (op_mode_ == OpMode::PRE_ACCEPT)
    {
        loop_pre_accept();
    }
    else if (op_mode_ == OpMode::ACCEPT)
    {
        auto t = micros();
        uart.loop(t);
    }
    else
    {
        ESP_LOGI(TAG, "Unknown mode !? op_mode_=%d", op_mode_);
    }
}

void UART::process(const UartMessageHolder &holder)
{
    ESP_LOGI(TAG, "process -> holder=?");
}

#endif

#endif