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
#define MODE_SHIFT 0
// speed -> total: 3 + 3 = 6 bits
#define SPEED_WIDTH 3 // the actual speeds are an array for wich this is the index
#define SPEED_SHIFT MODE_WIDTH
// steps -> total: 6 + 10 = 16 bits
#define STEPS_WIDTH 10
#define STEPS_SHIFT (SPEED_SHIFT + SPEED_WIDTH)

#define STEPS_MASK ((1 << STEPS_WIDTH) - 1)
#define MODE_MASK ((1 << MODE_WIDTH) - 1)
#define SPEED_MASK ((1 << SPEED_WIDTH) - 1)

struct InflatedCmdKey
{
    struct
    {
        // first 3 bits for mode
        uint16_t ghost : 1,
            clockwise : 1,
            absolute : 1,
            // next 3 bits for speed
            speed : SPEED_WIDTH,
                    // next 10 steps for steps
                    steps : STEPS_WIDTH;
        // which concludes: (1+1+1) + 3 + 10 = 16 bits
    } value;


    static const InflatedCmdKey& map(const uint16_t &raw)    {
        const InflatedCmdKey* keyPtr=(const InflatedCmdKey*)&raw;
        return *keyPtr;
    }

    inline bool clockwise() const
    {
        return value.clockwise;
    }

    bool absolute() const
    {
        return value.absolute;
    }

    inline bool swap_speed() const
    {
        return value.absolute;
    }

    inline bool ghost() const
    {
        return value.ghost;
    }

    inline int speed() const
    {
        return value.speed;
    }

    inline int steps() const
    {
        return value.steps;
    }
};

enum CmdSpecialMode
{
    FOLLOW_SECONDS_DISCRETE = ((1 << STEPS_WIDTH) - 1),
    FOLLOW_SECONDS = ((1 << STEPS_WIDTH) - 2)
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
            ESP_LOGE(TAG, "Invalid inflated speed!? speed=%d", value);
            return speeds[0];
        }
        return speeds[value];
    }
}
//TODO: do static :S
extern cmdSpeedUtil;

struct Cmd
{
    // first 3 bits for mode
    struct FatKey
    {
        uint32_t ghost : 1,
            clockwise : 1,
            absolute : 1,
            // next 3 bits for speed
            speed : 3 + 16,
            // next 10 steps for steps
            steps : 10;
    };

    struct ModeKey
    {
        uint32_t mode : 3,
            // next 3 bits for speed
            speed : 3 + 16,
            // next 10 steps for steps
            steps : 10;
    };
    union Value
    {
        FatKey fatKey;
        ModeKey modeKey;
        uint32_t rawKey;
    };
    Value value;

    //uint8_t mode;
    //uint8_t speed;
    //uint16_t steps;

public:
#ifdef MASTER_MODE

    CmdInt asRaw() const
    {

        return (static_cast<CmdInt>(value.modeKey.mode & MODE_MASK) << MODE_SHIFT) |
               (static_cast<CmdInt>(cmdSpeedUtil.inflate_speed(value.modeKey.speed) & SPEED_MASK) << SPEED_SHIFT) |
               (static_cast<CmdInt>(value.modeKey.steps & STEPS_MASK) << STEPS_SHIFT);
    }
#endif
    Cmd()
    {
        value.rawKey = 0;
    }

    void set_swap_bit()
    {
        value.fatKey.absolute = true;
    }

    inline bool clockwise() const
    {
        return value.fatKey.clockwise;
    }

    inline bool absolute() const
    {
        return value.fatKey.absolute;
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
        return value.fatKey.ghost;
    }

    inline int speed() const
    {
        return value.modeKey.speed;
    }

    inline int mode() const
    {
        return value.modeKey.mode;
    }

    inline int steps() const
    {
        return value.fatKey.steps;
    }


    Cmd(CmdInt raw)
    {
        InflatedCmdKey *rawPtr = (InflatedCmdKey *)&raw;
        value.fatKey.ghost = rawPtr->value.ghost;
        value.fatKey.clockwise = rawPtr->value.clockwise;
        value.fatKey.absolute = rawPtr->value.absolute;
        value.fatKey.speed = cmdSpeedUtil.deflate_speed(rawPtr->value.speed);
        value.fatKey.steps = rawPtr->value.steps;
    }

    Cmd(const InflatedCmdKey &raw)
    {
        value.fatKey.ghost = raw.value.ghost;
        value.fatKey.clockwise = raw.value.clockwise;
        value.fatKey.absolute = raw.value.absolute;
        value.fatKey.speed = cmdSpeedUtil.deflate_speed(raw.value.speed);
        value.fatKey.steps = raw.value.steps;
    }

#ifdef MASTER_MODE
    Cmd(CmdEnum mode, u16 steps, u8 speed)
    {
        value.modeKey.mode = (uint8_t)mode;
        value.modeKey.speed = speed;
        value.modeKey.steps = steps;
    }
    Cmd(uint8_t mode, u16 steps, u8 speed)
    {
        value.modeKey.mode = (uint8_t)mode;
        value.modeKey.speed = speed;
        value.modeKey.steps = steps;
    }
    double time() const
    {
        return static_cast<double>(value.modeKey.steps * 60) / static_cast<double>(abs(value.modeKey.speed)) / static_cast<double>(NUMBER_OF_STEPS);
    }
#endif
    bool isEmpty() const
    {
        return value.rawKey == 0;
    }
};