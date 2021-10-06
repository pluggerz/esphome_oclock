#pragma once

#define ESPHOME_MODE

typedef unsigned long Micros;

#define TAG __FILE__

#ifdef ESP8266
#define MASTER_MODE
#else
#define SLAVE_MODE
#undef ESPHOME_MODE
#endif

#ifdef ESPHOME_MODE

#include "esphome/core/log.h"
using namespace esphome;

#else
#define ESP_LOGI(tag, ...)
#define ESP_LOGD(tag, ...)
#define ESP_LOGW(tag, ...)
#define ESP_LOGCONFIG(tag, ...)
#endif

#include <Arduino.h>

typedef unsigned long Millis;

typedef void (*LoopFuncPtr)(Millis);
