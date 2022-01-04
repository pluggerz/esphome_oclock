#pragma once

enum CmdEnum
{
    NONE = 0,
    GHOST = 0x1,
    CLOCKWISE = 0x2,       // otherwise ANTI_CLOCKWISE
    ANTI_CLOCKWISE = NONE, // dont use for testing!
    // ABSOLUTE is only valid on the master (since we convert ABSOLUTE to RELATIVE for the slaves)
    ABSOLUTE = 0x4, // otherwise RELATIVE
    SWAP_SPEED = 0x4,
    RELATIVE = NONE, // dont use for testing!
    // note: we could do with only ABSOLUTE wich gives us a a free bit
};

typedef uint16_t CmdInt;

// mode ->  total: 3 bits
#define MODE_WIDTH 3
// speed -> total: 3 + 3 = 6 bits
#define SPEED_WIDTH 3 // the actual speeds are an array for wich this is the index
// steps -> total: 6 + 10 = 16 bits
#define STEPS_WIDTH 10

#define STEPS_MASK ((1 << STEPS_WIDTH) - 1)
#define MODE_MASK ((1 << MODE_WIDTH) - 1)
#define SPEED_MASK ((1 << SPEED_WIDTH) - 1)

union InflatedCmdKey
{
    struct
    {
        // first 3 bits for mode
        uint16_t ghost : 1,
            clockwise : 1,
            extended : 1,
            // next 3 bits for speed
            steps : 10,
            speed : 3;
        // which concludes: (1+1+1) + 3 + 10 = 16 bits
    } value;
    struct
    {
        uint16_t : 2,
            extended : 1, : 13;
    } extended_value;
    uint16_t raw;
    uint16_t mode_ : MODE_WIDTH;
    // uint32_t base_ : MODE_WIDTH + STEPS_WIDTH;

    InflatedCmdKey()
    {
        raw = 0;
    }

#ifdef ESP8266
    void dump(const char *extra) const
    {
        if (extended())
        {
            ESP_LOGE(TAG, "%s: ex=%s", extra, YESNO(extended()));
        }
        else
        {
            ESP_LOGE(TAG, "%s: gh=%s cl=%s, ex=%s, steps=%d", extra, YESNO(ghost()), YESNO(clockwise()), YESNO(extended()), steps());
        }
    }
#endif

    static const InflatedCmdKey &map(const uint16_t &raw)
    {
        const InflatedCmdKey *keyPtr = (const InflatedCmdKey *)&raw;
        return *keyPtr;
    }

    inline bool clockwise() const
    {
        return value.clockwise;
    }

    bool extended() const
    {
        return value.extended;
    }

    inline bool ghost() const
    {
        return value.ghost;
    }

    inline bool ghost_or_alike() const
    {
        return value.ghost || value.extended;
    }

    inline int inflated_speed() const
    {
        return value.speed;
    }

    inline int steps() const
    {
        return value.steps;
    }

    inline bool empty() const
    {
        return raw == 0;
    }
};

enum CmdSpecialMode
{
    FOLLOW_SECONDS_DISCRETE = 1,
    FOLLOW_SECONDS = 2
};

class CmdSpeedUtil
{
public:
    typedef uint8_t Speeds[8];
    const uint8_t max_inflated_speed = 7;

    Speeds speeds = {
        1,  // 0
        2,  // 1
        4,  // 2
        8,  // 3
        12, // 4
        16, // 5
        32, // 6
        64, // 7
    };

    void reset()
    {
        set_speeds({1, 2, 4, 8, 12, 16, 32, 64});
    }

    const Speeds &get_speeds() const;
    void set_speeds(const Speeds &speeds);

    uint8_t inflate_speed(const uint8_t value) const;
    inline uint8_t deflate_speed(const uint8_t value) const
    {
        if ((value & SPEED_MASK) != value)
        {
            ESP_LOGE(TAG, "Invalid!? inflated_speed=%d", value);
            return speeds[0];
        }
        return speeds[value];
    }
}
//TODO: do static :S
extern cmdSpeedUtil;

#ifdef MASTER_MODE
union DeflatedCmdKey
{
private:
    // first 3 bits for mode
    struct FatKey
    {
        uint32_t ghost : 1,
            clockwise : 1,
            absolute : 1,
            // next 10 steps for steps
            steps : 10,
            // next 3 and on bits for speed
            speed : 3 + 16;
    };

    uint32_t mode_ : MODE_WIDTH;
    FatKey fatKey;

public:
    uint32_t raw : 32;

    InflatedCmdKey asInflatedCmdKey() const
    {
        InflatedCmdKey ret;
        ret.mode_ = mode_;
        ret.value.steps = fatKey.steps;
        if (!extended())
            ret.value.speed = cmdSpeedUtil.inflate_speed(fatKey.speed);
        return ret;
    }

#ifdef ESP8266
    void dump(const char *extra) const
    {
        ESP_LOGE(TAG, "%s: gh=%s cl=%s, ab=%s, steps=%d, sp=%d raw=%d", extra, YESNO(ghost()), YESNO(clockwise()), YESNO(absolute()), steps(), speed(), (int)raw);
    }
#endif

    DeflatedCmdKey()
    {
        raw = 0;
    }

    DeflatedCmdKey(int _mode, u16 _steps, u8 _speed)
    {
        mode_ = (uint8_t)_mode;
        fatKey.speed = _speed;
        fatKey.steps = _steps;
    }

    inline void set_swap_bit()
    {
        fatKey.absolute = true;
    }

    inline bool clockwise() const
    {
        return fatKey.clockwise;
    }

    inline bool absolute() const
    {
        return fatKey.absolute;
    }

    inline bool extended() const
    {
        return fatKey.absolute;
    }

    inline bool relative() const
    {
        return !absolute();
    }

    inline bool swap_speed() const
    {
        return absolute();
    }

    inline bool ghost() const
    {
        return fatKey.ghost;
    }

    inline int speed() const
    {
        return fatKey.speed;
    }

    inline int mode() const
    {
        return mode_;
    }

    inline int steps() const
    {
        return fatKey.steps;
    }

    double time() const
    {
        return static_cast<double>(fatKey.steps * 60) / static_cast<double>(abs(fatKey.speed)) / static_cast<double>(NUMBER_OF_STEPS);
    }
};
#endif