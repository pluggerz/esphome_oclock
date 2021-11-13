#pragma once

#include "oclock.h"

namespace oclock
{
    namespace time_tracker
    {
        class Time;
        class TimeTracker;

        // implementations:
        class TestTimeTracker;
        extern TestTimeTracker testTimeTracker;

        class RealTimeTracker;
        extern RealTimeTracker realTimeTracker;

        class InternalClockTimeTracker;
        extern InternalClockTimeTracker internalClockTimeTracker;
    } // namespace time_tracker

} // namespace oclock

class oclock::time_tracker::Time final
{
public:
    /** seconds after the minute [0-60]
        * @note second is generally 0-59; the extra range is to accommodate leap seconds.
        **/
    uint8_t second{0};
    /// minutes after the hour [0-59]
    uint8_t minute{0};
    /// hours since midnight [0-23]
    uint8_t hour{0};
    /// day of the week; sunday=1 [1-7]
};

class oclock::time_tracker::TimeTracker
{
protected:
    const __FlashStringHelper *name_;

public:
    TimeTracker(const __FlashStringHelper *name) : name_(name)
    {
    }
    virtual Time now() const = 0;

    virtual int get_speed_multiplier() const = 0;

    const __FlashStringHelper *name() const
    {
        return name_;
    }

    virtual void dump_config(const char *tag) const
    {
        auto t0 = now();
        ESP_LOGI(tag, "     %s: (%02d:%02d:%02d)", name_, t0.hour, t0.minute, t0.second);
    }
};

class oclock::time_tracker::InternalClockTimeTracker final : public oclock::time_tracker::TimeTracker
{
    uint64_t currentEpocInSeconds{0};
    uint64_t lastUpdateInMillis{0};
    const uint8_t multiplier{3};

public:
    InternalClockTimeTracker() : TimeTracker(F("InternalClockTimeTracker")) {}

    virtual int get_speed_multiplier() const
    {
        return multiplier;
    }

    virtual void dump_config(const char *tag) const override
    {
        auto t0 = now();
        ESP_LOGI(tag, "     %s: (%02d:%02d:%02d) multiplier=%d", name_, t0.hour, t0.minute, t0.second, multiplier);
    }

    virtual Time now() const override final
    {
        unsigned long additionalMillis = ::millis() * multiplier - lastUpdateInMillis;
        unsigned long additionalSeconds = floor(additionalMillis / 1000.0);
        unsigned long rawTime = currentEpocInSeconds + additionalSeconds;

        Time time;
        time.hour = (rawTime % 86400L) / 3600;
        time.minute = (rawTime % 3600) / 60;
        time.second = rawTime % 60;
        return time;
    }
};

class oclock::time_tracker::TestTimeTracker final : public oclock::time_tracker::TimeTracker
{
    Time time_;

public:
    virtual int get_speed_multiplier() const
    {
        return 1;
    }
    TestTimeTracker() : TimeTracker(F("TestTimeTracker")) {}

    void set_hour(int value) { time_.hour = value; }
    void set_minute(int value) { time_.minute = value; }

    void set(const Time &time) { time_ = time; }
    Time get() const { return time_; }

    virtual Time now() const override final { return get(); }
};

#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"

class oclock::time_tracker::RealTimeTracker final : public oclock::time_tracker::TimeTracker
{
    esphome::time::RealTimeClock *time_ = nullptr;
    virtual int get_speed_multiplier() const
    {
        return 1;
    }

public:
    RealTimeTracker() : TimeTracker(F("RealTimeTracker")) {}
    void set(esphome::time::RealTimeClock *time) { time_ = time; }

    virtual Time now() const override final
    {
        if (time_ == nullptr)
        {
            return Time();
        }
        auto espNow = time_->now();
        Time ret;
        ret.second = espNow.second;
        ret.minute = espNow.minute;
        ret.hour = espNow.hour;
        return ret;
    }
};
#endif