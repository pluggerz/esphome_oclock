#pragma once

#include "oclock.h"

#ifdef SLAVE_MODE

#include <FastGPIO.h>
#define USE_FAST_GPIO_FOR_SYNC__

const uint8_t SYNC_OUT_PIN = 3; //PD3
const uint8_t SYNC_IN_PIN = 2;  //PD2

const uint8_t MOTOR_SLEEP_PIN = 4; //PD4
const uint8_t MOTOR_ENABLE = 10;   //PB2
const uint8_t MOTOR_RESET = 9;     //PB1

const uint8_t MOTOR_A_DIR = 5;  //PD5
const uint8_t MOTOR_A_STEP = 6; //PD6

const uint8_t MOTOR_B_DIR = 7;  //PD7
const uint8_t MOTOR_B_STEP = 8; //PB0

const uint8_t SLAVE_RS485_RXD_DPIN = 1; // PD1 RO !RXD
const uint8_t SLAVE_POS_A = 16;         // A2
const uint8_t SLAVE_POS_B = 17;         // A3
const uint8_t RS485_RE_PIN = 18;        // A4 // RE
const uint8_t RS485_DE_PIN = 19;        // A5 // DE

const uint8_t LED_DATA_PIN = 11;
const uint8_t LED_CLOCK_PIN = 13;

const uint8_t SLAVE_RS485_TXD_DPIN = 0; // PD0 DI !TXD

#endif

#ifdef MASTER_MODE

const int MASTER_GPI0_PIN = 0; //18; // GPI00
const int ESP_TXD_PIN = 1;     //22; // GPI01/TXD
const int RS485_RE_PIN = 2;    //17; // GPI02
const int ESP_RXD_PIN = 3;     //21; // GPI03/RXD
const int I2C_SDA_PIN = 4;     //19; // GPI04
const int I2C_SCL_PIN = 5;     //20; // GPI05
const int SYNC_OUT_PIN = 12;   // 6; // GPI12
const int SYNC_IN_PIN = 13;    // 7; // GPI13
const int GPIO_14 = 14;        // 5; // GPI14
const int RS485_DE_PIN = 15;   //16; // GPI15
const int USB_POWER_PIN = 16;  // 4; // GPI16

#endif

typedef unsigned long Millis;

class Sync
{
public:
  static void setup()
  {
    // configure pins
    pinMode(SYNC_IN_PIN, INPUT);
    pinMode(SYNC_OUT_PIN, OUTPUT);
    digitalWrite(SYNC_OUT_PIN, LOW);
  }

  static void sleep(Millis delta)
  {
    Millis t0 = ::millis();
    while (::millis() - t0 < delta)
    {
      // do nothing
      ::delay(1);
    }
    return;
  }

  static void write(int state)
  {
    ESP_LOGD(TAG, "Sync.write(%s)", ONOFF(state));
#ifdef USE_FAST_GPIO_FOR_SYNC
    FastGPIO::Pin<SYNC_OUT_PIN>::setOutputValue(state);
#else
    digitalWrite(SYNC_OUT_PIN, state);
#endif
    // slow !S
    sleep(30);
  }

  static int read()
  {
    int state;
#ifdef USE_FAST_GPIO_FOR_SYNC
    state = digitalRead(SYNC_IN_PIN);
    //TODO: does not work?? state = FastGPIO::Pin<SYNC_IN_PIN>::isOutputValueHigh();
#else
    state = digitalRead(SYNC_IN_PIN);
#endif
    ESP_LOGD(TAG, "Sync.read() = %s", ONOFF(state));
    return state;
  }
};