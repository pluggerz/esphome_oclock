#pragma once

enum CmdEnum
{
    NONE = 0,
    GHOST = 0x1,
    CLOCKWISE = 0x2,       // otherwise ANTI_CLOCKWISE
    ANTI_CLOCKWISE = NONE, // dont use for testing!
    // ABSOLUTE is only valid on the master (since we convert ABSOLUTE to RELATIVE for the slaves)
    ABSOLUTE = 0x4,        // otherwise RELATIVE
    SWAP_SPEED = 0x4,
    RELATIVE = NONE,       // dont use for testing!
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
    uint8_t mode;
    uint8_t speed;
    uint16_t steps;

public:
#ifdef MASTER_MODE
    CmdInt asRaw() const
    {
        return (static_cast<CmdInt>(mode & MODE_MASK) << MODE_SHIFT) |
               (static_cast<CmdInt>(cmdSpeedUtil.inflate_speed(speed) & SPEED_MASK) << SPEED_SHIFT) |
               (static_cast<CmdInt>(steps & STEPS_MASK) << STEPS_SHIFT);
    }
#endif
    Cmd() : mode(0), speed(0), steps(0)
    {
    }
    Cmd(CmdInt raw) : mode(static_cast<uint8_t>((raw >> MODE_SHIFT) & MODE_MASK)),
                      speed(cmdSpeedUtil.deflate_speed(static_cast<CmdInt>((raw >> SPEED_SHIFT) & SPEED_MASK))),
                      steps(static_cast<uint16_t>((raw >> STEPS_SHIFT) & STEPS_MASK))
    {
    }
#ifdef MASTER_MODE
    Cmd(CmdEnum mode, u16 steps, u8 speed) : mode((uint8_t)mode), speed(speed), steps(steps)
    {
    }
    Cmd(uint8_t mode, u16 steps, u8 speed) : mode(mode), speed(speed), steps(steps)
    {
    }
    double time() const
    {
        return static_cast<double>(steps * 60) / static_cast<double>(abs(speed)) / static_cast<double>(NUMBER_OF_STEPS);
    }
#endif
    bool isEmpty() const
    {
        return mode == 0 && speed == 0 && steps == 0;
    }
};