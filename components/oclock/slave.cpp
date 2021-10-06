#include "oclock.h"
#ifdef SLAVE_MODE

#include "async.h"
#include "stepper.h"

const int STEP_MULTIPLIER = 4;
int NUMBER_OF_STEPS = 720 * STEP_MULTIPLIER;
bool forwardPulse = false;
int slaveId = -2;

// SHORT
const int OFF = 13 - 30;
const int STEPS_TO_ZERO_1 = -(169 + 5 + OFF) * 4;
// LONG
const int STEPS_TO_ZERO_2 = +(138 - 3 - OFF) * 4;

#include "pins.h"

StepperImpl<MOTOR_A_STEP, MOTOR_A_DIR, SLAVE_POS_B> stepper1(NUMBER_OF_STEPS, STEPS_TO_ZERO_2);
StepperImpl<MOTOR_B_STEP, MOTOR_B_DIR, SLAVE_POS_A> stepper2(NUMBER_OF_STEPS, STEPS_TO_ZERO_1);

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

InteropRS485 uart(RS485_DE_PIN, RS485_RE_PIN);

#include "leds.h"

void initMotorPins()
{
    // set mode
    pinMode(MOTOR_SLEEP_PIN, OUTPUT);
    pinMode(MOTOR_ENABLE, OUTPUT);
    pinMode(MOTOR_RESET, OUTPUT);

    stepper1.setup();
    stepper2.setup();

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

void setup()
{
    LedUtil::debug(1);
    initMotorPins();

    uart.setup();

    change_to_init();
}

void loop()
{
    uart.loop();
    Micros now = micros();
    AsyncRegister::loop(now);
}

void handleReset()
{
    LedUtil::debug(2);

    // set high
    forwardPulse = false;
    Sync::write(HIGH);

    // reset
    // ledAnimation->setLayer(0, &backgroundLedLayer)->start();

    slaveId = -2;
    // preMain1->reset();
    // preMain2->reset();
}

void handleStart()
{
    LedUtil::debug(3);

    Sync::write(LOW);
}

void handleAccept(UartAcceptMessage *msg)
{
    LedUtil::debug(4);

    if (slaveId == -2 && Sync::read() == HIGH)
    {
      slaveId = msg->getAssignedId();

      // Make sure other slaves also receive time
      Sync::sleep(100);
      Sync::write(HIGH);
      Sync::sleep(10);
      uart.send(UartAcceptMessage(slaveId, slaveId + 2));
      uart.startReceiving();
    }
}

void handleDone()
{ 
    LedUtil::debug(5);

    while (digitalRead(SYNC_IN_PIN) == HIGH)
    {
      // wait
    }
    Sync::write(LOW);
    forwardPulse = true;
}

void change_to_init()
{
    forwardPulse = false;
    slaveId=-2;
    
    uart.startReceiving();

    // inform the master someone new is waiting...
    Sync::write(HIGH);
}

#endif