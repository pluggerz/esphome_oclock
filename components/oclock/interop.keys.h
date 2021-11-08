#pragma once

#include "ticks.h"
#include "interop.h"
#include "keys.h"

#define MAX_ANIMATION_KEYS 90
#define MAX_ANIMATION_KEYS_PER_MESSAGE 14 // MAX 14!

struct UartKeysMessage : public UartMessage
{
    // it would be tempting to use: Cmd cmds[MAX_ANIMATION_KEYS_PER_MESSAGE] ={}; but padding is messed up between esp and uno :()
    // note we control the bits better ;P
private:
    uint8_t _size;
    uint16_t cmds[MAX_ANIMATION_KEYS_PER_MESSAGE] = {};

public:
    UartKeysMessage(uint8_t destination_id, uint8_t _size) : UartMessage(-1, MSG_SEND_KEYS, destination_id), _size(_size)
    {
    }
    uint8_t size() const
    {
        return _size;
    }

    uint16_t &operator[](int idx)
    {
        return cmds[idx];
    }
} __attribute__((packed, aligned(1)));

/***
 * 
 * Essentially every minute we send animation keys.
 * Some type of animations depends on the seconds.
 * Ideally, a minute has 60 seconds, but some time could be lost in prepping.
 * So, this message will give the actual minute. 
 * Or at least the time when the master will probability send the next instructions.
 */
struct UartEndKeysMessage : public UartMessage
{
public:
    uint16_t numberOfMillisLeft;
    uint8_t turn_speed, turn_speed_steps;
    uint8_t speed_map[8];
    uint64_t speed_detection;

    UartEndKeysMessage(const uint8_t turn_speed, const uint8_t turn_speed_steps, const uint8_t (&speed_map)[8], uint64_t _speed_detection, uint16_t numberOfMillisLeft)
        : UartMessage(-1, MSG_END_KEYS, ALL_SLAVES),
          numberOfMillisLeft(numberOfMillisLeft),
          turn_speed(turn_speed),
          turn_speed_steps(turn_speed_steps),
          speed_detection(_speed_detection)
    {
        ESP_LOGE(TAG, "SPD: %llu", speed_detection);

        for (int idx = 0; idx < 8; ++idx)
        {
            this->speed_map[idx] = speed_map[idx];
        }
    }
} __attribute__((packed, aligned(1)));
