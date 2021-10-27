#include "oclock.h"

#include "steppers.h"
#include "steps_executor.h"
#include "async.h"

bool forwardPulse = false;
int slaveId = -2;
int nextSlaveId = -2;

#include "pins.h"

StepperImpl<MOTOR_A_STEP, MOTOR_A_DIR, SLAVE_POS_B> stepper1(NUMBER_OF_STEPS);
StepperImpl<MOTOR_B_STEP, MOTOR_B_DIR, SLAVE_POS_A> stepper2(NUMBER_OF_STEPS);

PreMainMode preMain1(stepper1, "main1", NUMBER_OF_STEPS / 2);
PreMainMode preMain2(stepper2, "main2", 0);

class AllSteppers
{
public:
    static bool isCalibrating()
    {
        return digitalRead(MOTOR_ENABLE) == HIGH;
    }
    static void startCalibrating()
    {
        digitalWrite(MOTOR_ENABLE, HIGH);
    }

    static void endCalibrating()
    {
        digitalWrite(MOTOR_ENABLE, LOW);
    }
};

#include "interop.h"

byte receiverBufferBytes[RECEIVER_BUFFER_SIZE];
auto receiverBuffer = Buffer(receiverBufferBytes, RECEIVER_BUFFER_SIZE);
RS485Gate<RS485_DE_PIN, RS485_RE_PIN> gate;
InteropRS485 uart(ALL_SLAVES, gate, receiverBuffer);

#include "slave/leds.h"

int LedUtil::level = -1;

#include "slave/backgroundleds.h"

LedAsync ledAsync;

// background layers
BackgroundLedAnimations::Fixed backgroundLedLayer;
BackgroundLedAnimations::Fixed offBackgroundLedLayer;
BackgroundLedAnimations::Xmas xmasLedLayer;
BackgroundLedAnimations::Rainbow rainbowLedLayer;

// can work on top of any layer
// FollowLightLedLayer followLightLedLayer;
// FollowHandlesLedLayer followHandlesLedLayer;

void initMotorPins()
{
    // set mode
    pinMode(MOTOR_SLEEP_PIN, OUTPUT);
    pinMode(MOTOR_ENABLE, OUTPUT);
    pinMode(MOTOR_RESET, OUTPUT);

    stepper1.setup();
    stepper2.setup();

    StepExecutors::setup(stepper1, stepper2);

    Sync::sleep(10);

    // set state
    digitalWrite(MOTOR_SLEEP_PIN, LOW);
    digitalWrite(MOTOR_ENABLE, HIGH);
    digitalWrite(MOTOR_RESET, LOW);

    // wait at least 1ms
    Sync::sleep(10);
    digitalWrite(MOTOR_SLEEP_PIN, HIGH);
    digitalWrite(MOTOR_ENABLE, LOW);
    digitalWrite(MOTOR_RESET, HIGH);

    // wait at least 200ns
    Sync::sleep(10);
}

void change_to_init();
void changeToMainTask();

class InternalResetChecker : public AsyncDelay
{
    Millis tStart{0L};
    bool enabled_{false};

public:
    InternalResetChecker() : AsyncDelay(100)
    {
    }

    void enable()
    {
        enabled_ = true;
        tStart = 0L;
    }

    void disable() { enabled_ = false; }

    virtual void step() override
    {
        if (!enabled_)
            return;

        if (Sync::read() == HIGH)
        {
            LedUtil::reset();
            change_to_init();
        }
    }
};

auto internalResetChecker = new InternalResetChecker();

void high_prio_work()
{
    Micros now = micros();
    preMain1.loop(now);
    preMain2.loop(now);
}

void setup()
{
    Sync::setup();

    LedUtil::debug(1);

    initMotorPins();

    uart.setup();
    uart.setOwnerId(60);
    Hal::set_yield_func(high_prio_work);

    LedUtil::debug(1);

    // note that deleting will be harmful, but actually will never happen!
    //AsyncRegister::anom(&preMain1);
    //AsyncRegister::anom(&preMain2);

    preMain1.setup(::micros());
    preMain2.setup(::micros());

    // forward the news that we are new
    Sync::write(HIGH);

    change_to_init();

    AsyncRegister::byName("resetChecker", internalResetChecker);
    AsyncRegister::byName("led", &ledAsync);
}

void pushLog(bool overflow, uint8_t part, uint8_t total_parts)
{
    UartLogMessage msg(slaveId, part, total_parts, overflow);
    int idx = 0;
    while (!logExtractor->empty() && idx < msg.length() - 1)
    {
        msg.buffer[idx++] = logExtractor->pop();
        msg.buffer[idx] = 0;
    }
    if (idx > 0)
    {
        uart.send(msg);
        uart.skip();
    }
}
void pushLogs()
{
    // make sure we do not 'loop' because of logging of underlying functions :S
    auto overflow = logExtractor->lock();
    uint8_t total_parts = 1 + (logExtractor->size() / (UartLogMessage::length() - 1));
    uint8_t part = 0;
    while (!logExtractor->empty())
    {
        pushLog(overflow, part++, total_parts);
        overflow = false;
    }
    logExtractor->unlock();
}

void do_slave_config_request(const UartSlaveConfigRequest *msg)
{
    LedUtil::debug(12);
    bool change = stepper1.set_offset_steps(msg->handle_offset0);
    change = stepper2.set_offset_steps(msg->handle_offset1) || change;
    change = preMain1.set_initial_ticks(msg->initial_ticks0) || change;
    change = preMain2.set_initial_ticks(msg->initial_ticks1) || change;

    ESP_LOGI(TAG, "handle_offset(%d, %d)",
             msg->handle_offset0, msg->handle_offset1);
    ESP_LOGI(TAG, "initial_ticks(%d, %d)",
             msg->initial_ticks0, msg->initial_ticks1);
    if (change)
    {
        preMain1.reset();
        preMain2.reset();
    }
}

void change_to_init()
{
    LedUtil::debug(2);
    internalResetChecker->disable();

    Sync::write(HIGH);
    slaveId = -2;

    auto listener = [](const UartMessage *msg)
    {
        LedUtil::debug(3);
        switch (msg->getMessageType())
        {
        case MsgType::MSG_ID_RESET:
            Sync::write(LOW);

            LedUtil::debug(4);
            return true;

        case MsgType::MSG_ID_ACCEPT:
            LedUtil::debug(6);
            if (Sync::read() == HIGH && slaveId == -2)
            {
                LedUtil::debug(8);
                slaveId = ((UartAcceptMessage *)msg)->getAssignedId();
                nextSlaveId = slaveId + 2;
                uart.setOwnerId(slaveId);
                Sync::write(HIGH);

                pushLogs();
                uart.send(UartAcceptMessage(slaveId, slaveId + 2));
                uart.start_receiving();

                LedUtil::debug(9);
            }
            else
            {
                LedUtil::debug(7);
            }
            return true;

        case MSG_SLAVE_CONFIG:
            do_slave_config_request(reinterpret_cast<const UartSlaveConfigRequest *>(msg));
            return true;

        case MsgType::MSG_ID_DONE:
        {
            Sync::write(LOW);
            auto doneMsg = reinterpret_cast<const UartDoneMessage *>(msg);
            if (doneMsg->assignedId == nextSlaveId)
                nextSlaveId = -1;
            LedUtil::debug(10);

            internalResetChecker->enable();
            changeToMainTask();
            return true;
        }

        default:
            LedUtil::debug(5);
            return false;
        };
    };
    uart.set_listener(listener);
    uart.start_receiving();
}

void dump_config()
{
    ESP_LOGI(TAG, "  slave: S%d", slaveId >> 1);
    ESP_LOGI(TAG, "    STEPS_TO_0(%d, %d)", stepper1.get_offset_steps(), stepper2.get_offset_steps());
}


void do_dump_logs_request(const UartMessage *msg)
{
    auto alsoConfig = reinterpret_cast<const UartDumpLogsRequest *>(msg)->dump_config;
    pushLogs();
    if (alsoConfig)
    {
        dump_config();
        pushLogs();
    }
    uart.send(UartDumpLogsRequest(alsoConfig, slaveId, nextSlaveId));
    uart.start_receiving();
}

void do_position_request(const UartMessage *msg)
{
    LedUtil::debug(12);
    auto stop = ((const UartPosRequest *)msg)->stop;

    if (stop)
    {
        preMain1.stop();
        preMain2.stop();
    }

    auto busy = preMain1.busy() || preMain2.busy();
    auto stepper1Ticks = preMain1.busy() ? Ticks::normalize(-stepper1.get_offset_steps()) : stepper1.ticks();
    auto stepper2Ticks = preMain2.busy() ? Ticks::normalize(-stepper2.get_offset_steps()) : stepper2.ticks();

    pushLogs();
    uart.send(UartPosRequest(stop, slaveId, nextSlaveId, stepper1Ticks / STEP_MULTIPLIER, stepper2Ticks / STEP_MULTIPLIER, !busy));
    uart.start_receiving();
}

void do_led_mode_request(const LedModeRequest *msg)
{
    ledAsync.set_brightness(msg->brightness);
    const auto &mode = msg->mode;

    switch (mode % 9)
    {
    case 0:
        offBackgroundLedLayer.start();
        ledAsync.set_led_layer(&offBackgroundLedLayer);
        break;

#define CASE_XMAS(CASE, WHAT)                                         \
    case CASE:                                                        \
        xmasLedLayer.setPattern(BackgroundLedAnimations::Xmas::WHAT); \
        ledAsync.set_led_layer(&xmasLedLayer);                        \
        break;

        CASE_XMAS(1, WarmWhiteShimmer);
        CASE_XMAS(2, RandomColorWalk);
        CASE_XMAS(3, TraditionalColors);
        CASE_XMAS(4, ColorExplosion);
        CASE_XMAS(5, Gradient);
        CASE_XMAS(6, BrightTwinkle);
        CASE_XMAS(7, Collision);

    case 8:
        ledAsync.set_led_layer(&rainbowLedLayer);
        break;

    default:
        break;
    }
}

void changeToMainTask()
{
    auto listener = [](const UartMessage *msg)
    {
        switch (msg->getMessageType())
        {
        case MsgType::MSG_BEGIN_KEYS:
            StepExecutors::process_begin_keys(msg);
            return true;

        case MsgType::MSG_SEND_KEYS:
            StepExecutors::process_add_keys(reinterpret_cast<const UartKeysMessage *>(msg));
            return true;

        case MsgType::MSG_END_KEYS:
            StepExecutors::process_end_keys(reinterpret_cast<const UartEndKeysMessage *>(msg));
            return true;

        case MSG_POS_REQUEST:
            do_position_request(msg);
            return true;

        case MSG_DUMP_LOG_REQUEST:
            do_dump_logs_request(msg);
            return true;

        case MSG_LED_MODE:
            do_led_mode_request(reinterpret_cast<const LedModeRequest *>(msg));
            return true;

        default:
            return false;
        }
    };
    uart.set_listener(listener);
    uart.start_receiving();
}

int count = 0;

void loop()
{
    Micros now = micros();
    preMain1.loop(now);
    preMain2.loop(now);

    if (count++ == 64)
    {
        // this way the motors will be able to use speed 64
        AsyncRegister::loop(now);
        uart.loop();

        count = 0;
    }
}