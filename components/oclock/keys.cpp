#include "oclock.h"
#include "keys.h"

CmdSpeedUtil cmdSpeedUtil;

const CmdSpeedUtil::Speeds &CmdSpeedUtil::get_speeds() const { return speeds; }

void CmdSpeedUtil::set_speeds(const CmdSpeedUtil::Speeds &speeds)
{
    for (int idx = 0; idx <= max_inflated_speed; ++idx)
    {
        auto value = speeds[idx];
        this->speeds[idx] = value;
    }
    ESP_LOGI(TAG, "Speeds(%d,%d,%d,%d,%d,%d,%d,%d)", speeds[0], speeds[1], speeds[2], speeds[3], speeds[4], speeds[5], speeds[6], speeds[7]);
}

uint8_t CmdSpeedUtil::inflate_speed(const uint8_t value) const
{
    uint8_t inflated_speed = max_inflated_speed;
    while (inflated_speed > 0)
    {
        if (value >= speeds[inflated_speed])
            return inflated_speed;
        inflated_speed--;
    }
    return 0;
}
