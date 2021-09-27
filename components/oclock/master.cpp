#include "oclock.h"

#ifdef MASTER_MODE

#include "master.h"
#include "uart.h"

#include "pins.h"

UART uart(RS485_DE_PIN, RS485_RE_PIN);
oclock::Master oclock::master;
bool scanForError = false;
int slaveIdCounter = -2;

#ifndef ESPHOME_MODE
void setup() {
    oclock::master.setup();
}

void loop() {
    oclock::master.loop();
}
#endif

enum OpMode
{
    UNDEFINED,
    INITIAL_DELAY,
    PRE_ACCEPT,
    ACCEPT
} op_mode_ = OpMode::UNDEFINED;

const char* toChar(OpMode op_mode)
{
    switch (op_mode)
    {
    case OpMode::UNDEFINED:
        return "UNDEFINED";
    case OpMode::INITIAL_DELAY:
        return "INITIAL_DELAY";
    case OpMode::PRE_ACCEPT:
        return "PRE_ACCEPT";
    case OpMode::ACCEPT:
        return "ACCEPT";
    };
    return "UNKNOWN!?";
}

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

    uart.startTransmitting();
    
    // all slaves will stop their work and set sync to high
    ESP_LOGI(TAG, "sendCommandIdReset -> send(UartMessage(-1, MSG_ID_RESET))");
    uart.send(UartMessage(-1, MSG_ID_RESET));
    
    // make sure there is enough time for everyone to act
    Sync::sleep(1000);

    uart.send(UartMessage(-1, MSG_ID_RESET));

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

void blink()    {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(200);
    digitalWrite(LED_BUILTIN, LOW);
    delay(500);
}

void blink(int count)   {
    while(count > 0)    {
        blink();
        count--;
    }
}

void oclock::Master::setup()
{
    op_mode_ = OpMode::INITIAL_DELAY;
    op_delay_t_millis = millis() + 1000;
    ESP_LOGI(TAG, "Change to OpMode::INITIAL_DELAY (op_delay_t_millis=%d)", op_delay_t_millis);
    
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
    //pinMode(USB_POWER_PIN, INPUT);*/

    uart.setup();

    // we ignore errors
    scanForError = false;
    // reset slaveIdCounter (every clock takes 2 ids so that is why it is -2)
    slaveIdCounter = -2;

    ESP_LOGI(TAG, "setup -> scanForError=false, slaveIdCounter=-2");
    ESP_LOGI(TAG, "setup -> Sync::read() = %s", Sync::read() ? "HIGH" : "LOW");

    // force a reset, set line to high ( error mode )
    // so everyone will do the same
    Sync::write(HIGH);

    
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
    } else if (op_mode_ == OpMode::INITIAL_DELAY)
    {
        auto t = millis();
        if (t < op_delay_t_millis)
            return;

        blink(5);

        // lets start
        sendCommandIdReset();
    } else if (op_mode_ == OpMode::PRE_ACCEPT)
    {
        loop_pre_accept();
    }
    else if (op_mode_ == OpMode::ACCEPT)
    {
        auto t = micros();
        uart.loop(t);
    } else {
        ESP_LOGI(TAG, "Unknown mode !? op_mode_=%d", op_mode_);
    }
}

void UART::process(const UartMessageHolder &holder)
{
    ESP_LOGI(TAG, "process -> holder=?");
}

void oclock::Master::dump_config()
{
    ESP_LOGCONFIG(TAG, "MASTER:");
    ESP_LOGCONFIG(TAG, "   mode: %s", toChar(op_mode_));   
    uart.dump_config();
}

#endif