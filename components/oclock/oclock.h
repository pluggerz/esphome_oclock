#pragma once

#define ESPHOME_MODE

typedef unsigned long Micros;

#ifdef ESP8266

#include "esphome/core/defines.h"

#define MASTER_MODE
#define TAG __FILE__
#else
#define SLAVE_MODE
#undef ESPHOME_MODE

// a trick to move to FLASH instead of RAM and ignore the full path
#define TAG extractFileName(F(__FILE__))
#endif

#include "Arduino.h"

#ifdef ESPHOME_MODE

#include "esphome/core/log.h"

using namespace esphome;

#define NUMBER_OF_STEPS 720

#else

#include "slave/log.h"

#define STEP_MULTIPLIER 4
#define NUMBER_OF_STEPS (720 * STEP_MULTIPLIER)

#endif

typedef unsigned long Millis;

typedef void (*LoopFuncPtr)(Millis);

#define RECEIVER_BUFFER_SIZE 110 // was 128
#define MAX_SLAVES 24
#define MAX_HANDLES 48
