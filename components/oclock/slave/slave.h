#pragma once

#include "enums.h"

class SlaveSettings
{
    oclock::BackgroundEnum background_mode_ = oclock::BackgroundEnum::First;
    oclock::ForegroundEnum foreground_mode_ = oclock::ForegroundEnum::First;

    // 0..5: is managed by the class LedAsync
    int scaled_brightness_ = 5;

public:
    int get_scaled_brightness() const
    {
        return scaled_brightness_;
    }

    void set_scaled_brightness(int value);

    oclock::BackgroundEnum get_background_mode() const
    {
        return background_mode_;
    }
    void set_background_mode(oclock::BackgroundEnum value);

    oclock::ForegroundEnum get_foreground_mode() const
    {
        return foreground_mode_;
    }
    void set_foreground_mode(oclock::ForegroundEnum value);
} extern slave_settings;