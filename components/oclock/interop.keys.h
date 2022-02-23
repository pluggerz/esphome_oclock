#pragma once

#include "oclock.h"
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

    void set_key(int idx, uint16_t value)
    {
        cmds[idx] = value;
    }

    uint16_t get_key(int idx) const
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
    uint32_t number_of_millis_left;
    uint8_t turn_speed, turn_steps;
    uint8_t speed_map[8];
    uint64_t speed_detection;

    UartEndKeysMessage(const uint8_t turn_speed, const uint8_t turn_steps, const uint8_t (&speed_map)[8], uint64_t _speed_detection, uint32_t number_of_millis_left)
        : UartMessage(-1, MSG_END_KEYS, ALL_SLAVES),
          number_of_millis_left(number_of_millis_left),
          turn_speed(turn_speed),
          turn_steps(turn_steps),
          speed_detection(_speed_detection)
    {
        for (int idx = 0; idx < 8; ++idx)
        {
            this->speed_map[idx] = speed_map[idx];
        }
    }
} __attribute__((packed, aligned(1)));
