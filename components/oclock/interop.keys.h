#pragma once

#include "ticks.h"
#include "interop.h"
#include "keys.h"

#define MAX_ANIMATION_KEYS 20
#define MAX_ANIMATION_KEYS_PER_MESSAGE 14 // MAX 14!

struct UartKeysMessage : public UartMessage
{
    // it would be tempting to use: Cmd cmds[MAX_ANIMATION_KEYS_PER_MESSAGE] ={}; but padding is messed up between esp and uno :()
    // note we control the bits better ;P
private:
    u8 _size;
    CmdInt cmds[MAX_ANIMATION_KEYS_PER_MESSAGE] = {};

public:
    UartKeysMessage(u8 destination_id, u8 _size) : UartMessage(-1, MSG_SEND_KEYS, destination_id), _size(_size)
    {
    }
    u8 size() const
    {
        return _size;
    }

    CmdInt operator[](int idx) const
    {
        return cmds[idx];
    }

#ifdef MASTER_MODE
    void setCmd(int idx, const Cmd &cmd)
    {
        cmds[idx] = cmd.asRaw();
    }
#endif
    Cmd getCmd(int idx) const
    {
        return Cmd(cmds[idx]);
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
    u16 numberOfMillisLeft;
    uint8_t speed_map[8];

    UartEndKeysMessage(const uint8_t (&speed_map)[8], u16 numberOfMillisLeft) : UartMessage(-1, MSG_END_KEYS, ALL_SLAVES), numberOfMillisLeft(numberOfMillisLeft)
    {
        for (int idx = 0; idx < 8; ++idx)
        {
            this->speed_map[idx] = speed_map[idx];
        }
    }
} __attribute__((packed, aligned(1)));
