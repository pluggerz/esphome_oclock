#include "oclock.h"

#include "enums.h"
#include "slave.h"

SlaveSettings slave_settings;

#include "steppers.h"
#include "steps_executor.h"

bool forwardPulse = false;
int8_t slaveId = -2;
int8_t nextSlaveId = -2;

#include "pins.h"

Stepper0 stepper0(NUMBER_OF_STEPS);
Stepper1 stepper1(NUMBER_OF_STEPS);

PreMainMode<Stepper0> preMain0(stepper0, NUMBER_OF_STEPS / 2);
PreMainMode<Stepper1> preMain1(stepper1, 0);

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

#include "slave/leds_background.h"

LedAsync ledAsync;

BackgroundLedAnimations::Xmas xmasLedLayer;
BackgroundLedAnimations::Rainbow rainbowLedLayer;

#include "slave/leds_foreground.h"

FollowHandlesLedLayer followHandlesLedLayer;

// can work on top of any layer
// FollowLightLedLayer followLightLedLayer;
// FollowHandlesLedLayer followHandlesLedLayer;

void initMotorPins()
{
    // set mode
    pinMode(MOTOR_SLEEP_PIN, OUTPUT);
    pinMode(MOTOR_ENABLE, OUTPUT);
    pinMode(MOTOR_RESET, OUTPUT);

    stepper0.setup();
    stepper1.setup();

    StepExecutors::setup(stepper0, stepper1);

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

class InternalResetChecker final
{
    Micros t0_{0};
    Micros delay{200};
    bool enabled_{false};

    void step()
    {
        if (!enabled_)
            return;

        if (Sync::read() == HIGH)
        {
            LedUtil::reset();
            uart.downgrade_baud_rate();
            change_to_init();
        }
    }

public:
    void enable()
    {
        enabled_ = true;
    }

    void disable() { enabled_ = false; }

    void loop(Micros t1)
    {
        if (t1 - t0_ > delay)
        {
            t0_ = t1;
            step();
        }
    }
};

InternalResetChecker internalResetChecker;

void high_prio_work();

void test_solid_lights(const rgb_color &color)
{
    ledStrip.startFrame();
    for (auto i = 1; i <= LED_COUNT; i++)
    {
        ledStrip.sendColor(color, 31);
    }
    ledStrip.endFrame(LED_COUNT);
}

void test_sync()
{
    // make sure we are high

    test_solid_lights(rgb_color(0x00, 0xFF, 0xFF));
    Sync::write(LOW);
    Sync::sleep(200);

    // make sure there is enough time for everyone to act
    test_solid_lights(rgb_color(0xFF, 0x30, 0x10));
    Sync::write(HIGH);
    Sync::sleep(200);
}

void test_send()
{
    // receive (BLUE)
    test_solid_lights(rgb_color(0x00, 0x00, 0xFF));
    uart.start_receiving();
    Sync::sleep(1000);

    // send (GREEN)
    test_solid_lights(rgb_color(0x00, 0xFF, 0x00));
    uart.start_transmitting();
    Sync::sleep(1000);

    test_solid_lights(rgb_color(0xFF, 0x00, 0x00));
    auto msg = UartAcceptMessage(0, 1);
    uart.send(msg);
    Sync::sleep(1000);
}

template <class S>
void test_stepper(S &s)
{
    auto speed = 12;

    s.set_speed_in_revs_per_minute(speed);
    test_solid_lights(rgb_color(0x00, 0xFF, 0x00));
    s.step(+NUMBER_OF_STEPS / 4);
    test_solid_lights(rgb_color(0xFF, 0x00, 0x00));
    s.step(-NUMBER_OF_STEPS / 4);

    test_solid_lights(rgb_color(0x00, 0x00, 0xFF));
    delay(500);

    s.step(-NUMBER_OF_STEPS / 6);

    AllSteppers::startCalibrating();
    while (!s.is_magnet_tick())
    {
        test_solid_lights(rgb_color(0xFF, 0x00, 0xFF));
        delay(50);

        test_solid_lights(rgb_color(0x22, 0x00, 0xFF));
        delay(50);
    }
    AllSteppers::endCalibrating();
    test_solid_lights(rgb_color(0x00, 0xFF, 0x00));
    delay(500);
    s.step(NUMBER_OF_STEPS / 6);

    // find zero
    test_solid_lights(rgb_color(0xFF, 0xFF, 0x00));
    delay(500);

    bool found = false;
    for (int step = 0; step < NUMBER_OF_STEPS; ++step)
    {
        s.step(1);
        if (s.is_magnet_tick())
        {
            found = true;
            break;
        }
    }

    for (int step = 0; step < 4; ++step)
    {

        test_solid_lights(rgb_color(0xFF, 0xFF, 0x00));
        delay(500);

        test_solid_lights(found ? rgb_color(0x00, 0xFF, 0x00) : rgb_color(0xFF, 0x00, 0x00));
        delay(500);
    }
}

void test_lights_for_color(const rgb_color &color, const rgb_color &back_color)
{
    // moving RGB
    for (auto j = 1; j <= LED_COUNT; j++)
    {
        ledStrip.startFrame();
        for (auto i = 1; i <= LED_COUNT; i++)
        {
            if (i <= j)
                ledStrip.sendColor(color, 31);
            else
                ledStrip.sendColor(back_color, 31);
        }
        ledStrip.endFrame(LED_COUNT);
        delay(250);
    }
}

void test_lights()
{
    // initialize to white
    test_solid_lights(rgb_color(0xFF, 0xFF, 0xFF));
    delay(2000);
    test_lights_for_color(rgb_color(0xFF, 0x00, 0x00), rgb_color(0xFF, 0xFF, 0xFF));
    test_lights_for_color(rgb_color(0x00, 0xFF, 0x00), rgb_color(0xFF, 0x00, 0x00));
    test_lights_for_color(rgb_color(0x00, 0x00, 0xFF), rgb_color(0x00, 0xFF, 0x00));
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

    preMain0.setup(::micros());
    preMain1.setup(::micros());

    // forward the news that we are new ('error')
    Sync::write(HIGH);

    /*
    while (true)
   {
       test_lights();
       for (int syncs = 0; syncs < 10; ++syncs)
           test_sync();
       for (int syncs = 0; syncs < 10; ++syncs)
           test_send();

       test_stepper(stepper0);
       test_stepper(stepper1);
   }*/

    change_to_init();

    //  ledAsync.set_led_layer(&followHandlesLedLayer);
    ledAsync.set_foreground_led_layer(&debugLedLayer());
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
    bool change = stepper0.set_offset_steps(msg->handle_offset0);
    change = stepper1.set_offset_steps(msg->handle_offset1) || change;
    change = preMain0.set_initial_ticks(msg->initial_ticks0) || change;
    change = preMain1.set_initial_ticks(msg->initial_ticks1) || change;

    ESP_LOGI(TAG, "handle_off(%d, %d)",
             msg->handle_offset0, msg->handle_offset1);
    ESP_LOGI(TAG, "initial_ticks(%d, %d)",
             msg->initial_ticks0, msg->initial_ticks1);
    // if (change)
    {
        preMain0.reset();
        preMain1.reset();
    }
}

void change_to_init()
{
    LedUtil::debug(2);
    internalResetChecker.disable();

    cmdSpeedUtil.reset();
    StepExecutors::reset();

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
            // wait until premains are done
            while (preMain0.busy() || preMain1.busy())
            {
                auto now = ::micros();
                preMain0.loop(now);
                preMain1.loop(now);
            }

            LedUtil::debug(10);
            auto doneMsg = reinterpret_cast<const UartDoneMessage *>(msg);
            if (doneMsg->assignedId == nextSlaveId)
                nextSlaveId = -1;

            uart.upgrade_baud_rate(doneMsg->baud_rate);
            uart.start_receiving();
            Sync::write(LOW);

            delay(50);

            LedUtil::debug(11);
            internalResetChecker.enable();
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
    auto tag = F("");
    ESP_LOGI(tag, "slave: S%d", slaveId >> 1);
    ESP_LOGI(tag, " steppers:");
    stepper0.dump_config(tag);
    stepper1.dump_config(tag);
}

void do_inform_stop_animation()
{
    StepExecutors::request_stop();
}

void do_wait_for_animation()
{
    while (StepExecutors::active())
    {
        loop();
    }
    delay(5); // FIX CRC ERRORS?
    uart.send(UartWaitUntilAnimationIsDoneRequest(slaveId, nextSlaveId));
    uart.start_receiving();
}

void do_dump_logs_request(const UartDumpLogsRequest *msg)
{
    auto alsoConfig = msg->dump_config;
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
        preMain0.stop();
        preMain1.stop();
    }

    auto busy = preMain0.busy() || preMain1.busy();
    auto stepper0Ticks = preMain0.busy() ? Ticks::normalize(-stepper0.get_offset_steps()) : stepper0.ticks();
    auto stepper1Ticks = preMain1.busy() ? Ticks::normalize(-stepper1.get_offset_steps()) : stepper1.ticks();

    pushLogs();
    delay(5); // FIX CRC ERRORS?
    uart.send(UartPosRequest(stop, slaveId, nextSlaveId, stepper0Ticks / STEP_MULTIPLIER, stepper1Ticks / STEP_MULTIPLIER, !busy));
    uart.start_receiving();
}

void do_rgb_leds(const UartRgbBackgroundLedsMessage *msg)
{
    rgbLedBackgroundLayer(msg->leds);
    slave_settings.set_background_mode(oclock::BackgroundEnum::RgbColors);
}

void do_rgb_leds(const UartRgbForegroundLedsMessage *msg)
{
    rgbLedForegroundLayer(msg->leds);
    slave_settings.set_foreground_mode(oclock::ForegroundEnum::RgbColors);
}

void do_led_background_mode_request(const LedModeRequest *msg)
{
    slave_settings.set_background_mode(msg->get_background_enum());
}

void SlaveSettings::set_background_mode(oclock::BackgroundEnum value)
{
    background_mode_ = value;
    switch (background_mode_)
    {
    case oclock::BackgroundEnum::RgbColors:
    {
        auto &layer = rgbLedBackgroundLayer();
        layer.start();
        ledAsync.set_led_layer(&layer);
        break;
    }

    case oclock::BackgroundEnum::SolidColor:
        // ignored
        ESP_LOGI(TAG, "bg.sc");
        break;

#define CASE_XMAS(CASE, WHAT)                                         \
    case oclock::BackgroundEnum::CASE:                                \
        xmasLedLayer.setPattern(BackgroundLedAnimations::Xmas::WHAT); \
        ledAsync.set_led_layer(&xmasLedLayer);                        \
        break;

        CASE_XMAS(WarmWhiteShimmer, WarmWhiteShimmer);
        CASE_XMAS(RandomColorWalk, RandomColorWalk);
        CASE_XMAS(TraditionalColors, TraditionalColors);
        CASE_XMAS(ColorExplosion, ColorExplosion);
        CASE_XMAS(Gradient, Gradient);
        CASE_XMAS(BrightTwinkle, BrightTwinkle);
        CASE_XMAS(Collision, Collision);

    case oclock::BackgroundEnum::Rainbow:
        ledAsync.set_led_layer(&rainbowLedLayer);
        break;

    default:
        break;
    }
}

void do_led_foreground_mode_request(const LedModeRequest *msg)
{
    slave_settings.set_foreground_mode(msg->get_foreground_enum());
}

void SlaveSettings::set_foreground_mode(oclock::ForegroundEnum value)
{
    foreground_mode_ = value;
    switch (foreground_mode_)
    {
    case oclock::ForegroundEnum::RgbColors:
    {
        ESP_LOGI(TAG, "fg.rgb");
        auto &layer = rgbLedForegroundLayer();
        layer.start();
        ledAsync.set_foreground_led_layer(&layer);
        break;
    }

    case oclock::ForegroundEnum::DebugLeds:
        ESP_LOGI(TAG, "fg.debug");
        ledAsync.set_foreground_led_layer(&debugLedLayer());
        return;

    case oclock::ForegroundEnum::FollowHandles:
        ESP_LOGI(TAG, "fg.follow");
        ledAsync.set_foreground_led_layer(&followHandlesLayer(false));
        return;

    case oclock::ForegroundEnum::None:
        ESP_LOGI(TAG, "fg.0");
        ledAsync.set_foreground_led_layer(nullptr);
        return;

    default:
        ESP_LOGI(TAG, "fg.IGN");
        return;
    }
}

void do_led_mode_request(const LedModeRequest *msg)
{
    if (msg->foreground)
        do_led_foreground_mode_request(msg);
    else
        do_led_background_mode_request(msg);
}

void do_color_request(const UartColorMessage *msg)
{
    rgbLedBackgroundLayer(msg->color);
}

void do_calibrate(bool calibrate);

void do_brightness_request(const UartScaledBrightnessMessage *msg)
{
    slave_settings.set_scaled_brightness(msg->scaled_brightness);
}

void SlaveSettings::set_scaled_brightness(int value)
{
    if (scaled_brightness_ == value)
        return;

    scaled_brightness_ = value;
    ledAsync.updateLeds();
}

auto uartMainListener = [](const UartMessage *msg)
{
    switch (msg->getMessageType())
    {
    case MsgType::MSG_BRIGHTNESS:
        do_brightness_request(reinterpret_cast<const UartScaledBrightnessMessage *>(msg));
        return true;

    case MsgType::MSG_BOOL_DEBUG_LED_LAYER:
        if (reinterpret_cast<const UartBoolMessage *>(msg)->value)
        {
            ledAsync.set_foreground_led_layer(&debugLedLayer());
        }
        else
        {
            ledAsync.set_foreground_led_layer(nullptr);
        }
        return true;

    case MsgType::MSG_COLOR:
        do_color_request(reinterpret_cast<const UartColorMessage *>(msg));
        return true;

    case MsgType::MSG_CALIBRATE_START:
        do_calibrate(true);
        return true;

    case MsgType::MSG_CALIBRATE_END:
        do_calibrate(false);
        return true;

    case MsgType::MSG_BEGIN_KEYS:
        StepExecutors::process_begin_keys(msg);
        return true;

    case MsgType::MSG_SEND_KEYS:
        StepExecutors::process_add_keys(reinterpret_cast<const UartKeysMessage *>(msg));
        return true;

    case MsgType::MSG_END_KEYS:
        StepExecutors::process_end_keys(slaveId, reinterpret_cast<const UartEndKeysMessage *>(msg));
        return true;

    case MSG_POS_REQUEST:
        do_position_request(msg);
        return true;

    case MSG_DUMP_LOG_REQUEST:
        do_dump_logs_request(reinterpret_cast<const UartDumpLogsRequest *>(msg));
        return true;

    case MSG_WAIT_FOR_ANIMATION:
        do_wait_for_animation();
        return true;

    case MSG_INFORM_STOP_ANIMATION:
        do_inform_stop_animation();
        return true;

    case MSG_LED_MODE:
        do_led_mode_request(reinterpret_cast<const LedModeRequest *>(msg));
        return true;

    case MSG_BACKGROUND_RGB_LEDS:
        do_rgb_leds(reinterpret_cast<const UartRgbBackgroundLedsMessage *>(msg));
        return true;

    case MSG_FOREGROUND_RGB_LEDS:
        do_rgb_leds(reinterpret_cast<const UartRgbForegroundLedsMessage *>(msg));
        return true;

    default:
        return false;
    }
};

void changeToMainTask()
{
    uart.set_listener(uartMainListener);
    uart.start_receiving();
}

bool is_calibrating = false;

enum class LoopMode
{
    STEP_EXECUTOR,
    CALIBRATE_FIND,
    CALIBRATE_MANUAL
};

LoopMode loop_mode_ = LoopMode::STEP_EXECUTOR;

inline void high_prio_work(Micros now)
{
    if (loop_mode_ == LoopMode::STEP_EXECUTOR)
    {
        preMain0.loop(now);
        preMain1.loop(now);
    }
    else if (loop_mode_ == LoopMode::CALIBRATE_FIND)
    {
        int goal = NUMBER_OF_STEPS / 2;
        if (stepper0.ticks() != goal)
            stepper0.tryToStep(now);
        if (stepper1.ticks() != goal)
            stepper1.tryToStep(now);
        if (stepper0.ticks() == goal && stepper1.ticks() == goal)
        {
            loop_mode_ = LoopMode::CALIBRATE_MANUAL;
            AllSteppers::startCalibrating();
            ESP_LOGI(TAG, "LM = M");
        }
    }
}

inline void high_prio_work()
{
    high_prio_work(micros());
}

void do_calibrate(bool calibrate)
{
    if (calibrate)
    {
        stepper0.set_current_speed_in_revs_per_minute(12);
        stepper1.set_current_speed_in_revs_per_minute(12);
        loop_mode_ = LoopMode::CALIBRATE_FIND;
        ESP_LOGI(TAG, "LM = F");
    }
    else
    {
        loop_mode_ = LoopMode::STEP_EXECUTOR;
        AllSteppers::endCalibrating();
        ESP_LOGI(TAG, "LM = ST");
    }
}

void loop()
{
    Micros now = micros();
    high_prio_work(now);

    // this way the motors will be able to use speed 64
    uart.loop();
    internalResetChecker.loop(now);
    ledAsync.loop(now);
}