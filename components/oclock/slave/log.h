#pragma once

#include <WString.h>

// we mimic ESPHOME logging to easy the job
#define HOT __attribute__((hot))

#define ESPHOME_LOG_LEVEL_NONE 0
#define ESPHOME_LOG_LEVEL_ERROR 1
#define ESPHOME_LOG_LEVEL_WARN 2
#define ESPHOME_LOG_LEVEL_INFO 3
#define ESPHOME_LOG_LEVEL_CONFIG 4
#define ESPHOME_LOG_LEVEL_DEBUG 5
#define ESPHOME_LOG_LEVEL_VERBOSE 6
#define ESPHOME_LOG_LEVEL_VERY_VERBOSE 7

#ifndef ESPHOME_LOG_LEVEL
#define ESPHOME_LOG_LEVEL ESPHOME_LOG_LEVEL_INFO
#endif

class LogExtractor
{
public:
    virtual char pop() = 0;
    virtual uint16_t size() const = 0;

    /**
     *  lock() returns true if there was an overflow (deleted logs) or not
     */
    virtual bool lock() = 0;
    virtual void unlock() = 0;

    bool empty() const { return size() == 0; }
};

extern LogExtractor *logExtractor;

const char *extractFileName(const __FlashStringHelper *const _path);

HOT void esp_log_printf_(int level, const void *tag, int line, const __FlashStringHelper *format, ...);

// #define esph_log_e(tag, format, ...)
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_ERROR
#define esph_log_e(tag, format, ...) \
    esp_log_printf_(ESPHOME_LOG_LEVEL_ERROR, tag, __LINE__, ESPHOME_LOG_FORMAT(format), ##__VA_ARGS__)
#else
#define esph_log_e(tag, format, ...)
#endif

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_WARN
#define esph_log_w(tag, format, ...) \
    esp_log_printf_(ESPHOME_LOG_LEVEL_WARN, tag, __LINE__, ESPHOME_LOG_FORMAT(format), ##__VA_ARGS__)
#else
#define esph_log_w(tag, format, ...)
#endif

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_INFO
#define esph_log_i(tag, format, ...) \
    esp_log_printf_(ESPHOME_LOG_LEVEL_INFO, tag, __LINE__, ESPHOME_LOG_FORMAT(format), ##__VA_ARGS__)
#else
#define esph_log_i(tag, format, ...)
#endif

#define esph_log_d(tag, format, ...)
#define esph_log_vv(tag, format, ...)
#define esph_log_config(tag, format, ...)


#define ESP_LOGE(tag, ...) esph_log_e(tag, __VA_ARGS__)
#define LOG_E(tag, ...) ESP_LOGE(tag, __VA__ARGS__)
#define ESP_LOGW(tag, ...) esph_log_w(tag, __VA_ARGS__)
#define LOG_W(tag, ...) ESP_LOGW(tag, __VA__ARGS__)
#define ESP_LOGI(tag, ...) esph_log_i(tag, __VA_ARGS__)
#define LOG_I(tag, ...) ESP_LOGI(tag, __VA__ARGS__)
#define ESP_LOGD(tag, ...) esph_log_d(tag, __VA_ARGS__)
#define LOG_D(tag, ...) ESP_LOGD(tag, __VA__ARGS__)
#define ESP_LOGCONFIG(tag, ...) esph_log_config(tag, __VA_ARGS__)
#define LOG_CONFIG(tag, ...) ESP_LOGCONFIG(tag, __VA__ARGS__)
#define ESP_LOGV(tag, ...) esph_log_v(tag, __VA_ARGS__)
#define LOG_V(tag, ...) ESP_LOGV(tag, __VA__ARGS__)
#define ESP_LOGVV(tag, ...) esph_log_vv(tag, __VA_ARGS__)
#define LOG_VV(tag, ...) ESP_LOGVV(tag, __VA__ARGS__)

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)                                                                                    \
    ((byte)&0x80 ? '1' : '0'), ((byte)&0x40 ? '1' : '0'), ((byte)&0x20 ? '1' : '0'), ((byte)&0x10 ? '1' : '0'), \
        ((byte)&0x08 ? '1' : '0'), ((byte)&0x04 ? '1' : '0'), ((byte)&0x02 ? '1' : '0'), ((byte)&0x01 ? '1' : '0')
#define YESNO(b) ((b) ? F("YES") : F("NO"))
#define ONOFF(b) ((b) ? F("ON") : F("OFF"))
#define TRUEFALSE(b) ((b) ? F("TRUE") : F("FALSE"))

#define ESPHOME_LOG_FORMAT(format) F(format)