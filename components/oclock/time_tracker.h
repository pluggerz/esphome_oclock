#pragma once

#include "oclock.h"

namespace oclock
{
    namespace time_tracker
    {
        class Text;
        class TextTracker;

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

struct oclock::time_tracker::Text 
{
    char ch0='*', ch1='*', ch2='*', ch3='*';
    long millis_left;

    bool __equal__(const Text &t) const {
        return ch0 == t.ch0 && ch1 == t.ch1 && ch2 == t.ch2 && ch3 == t.ch3;
    }

    void all(char ch) {
        ch0=ch1=ch2=ch3=ch;
    }
};

struct oclock::time_tracker::Time final
{
    /** seconds after the minute [0-60]
     * @note second is generally 0-59; the extra range is to accommodate leap seconds.
     **/
    uint8_t second{0};
    /// minutes after the hour [0-59]
    uint8_t minute{0};
    /// hours since midnight [0-23]
    uint8_t hour{0};
    /// day of the week; sunday=1 [1-7]
    
    Text to_text(float millis_left) const
    {
        Text text;
        text.millis_left = millis_left;
        text.ch0 = '0' + (hour / 10);
        text.ch1 = '0' + (hour % 10);
        text.ch2 = '0' + (minute / 10);
        text.ch3 = '0' + (minute % 10);
        return text;
    }
};

class oclock::time_tracker::TextTracker
{
protected:
    const __FlashStringHelper *name_;

public:
    TextTracker(const __FlashStringHelper *name) : name_(name)
    {
    }

    virtual Text to_text() const;
    
    virtual int get_speed_multiplier() const = 0;

    const __FlashStringHelper *name() const
    {
        return name_;
    }

    virtual void dump_config(const char *tag) const
    {
        auto t = to_text();
        ESP_LOGI(tag, "     %s: text('%c','%c','%c','%c')", name_, t.ch0, t.ch1, t.ch2, t.ch3);
    }
};

class oclock::time_tracker::TimeTracker : public TextTracker
{

public:
    TimeTracker(const __FlashStringHelper *name) : TextTracker(name)
    {
    }
    virtual Time now() const = 0;

    virtual Text to_text() const override   {
        const auto stamp=now();
        float millis_left = 1000.0f * (60.0f - stamp.second) / float(get_speed_multiplier());
        return stamp.to_text(millis_left);
    }

    virtual int get_speed_multiplier() const = 0;

    virtual void dump_config(const char *tag) const override
    {
        auto t0 = now();
        ESP_LOGI(tag, "     %s: time(%02d:%02d:%02d)", name_, t0.hour, t0.minute, t0.second);
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


// TODO: TestTimeTracker -> TestTextracker
class oclock::time_tracker::TestTimeTracker final : public oclock::time_tracker::TextTracker
{
    Text text_;

public:
    virtual int get_speed_multiplier() const
    {
        return 1;
    }
    TestTimeTracker() : TextTracker(F("TestTimeTracker")) {}

    void set(const Text &value) { text_ = value; }
    Text get() const { return text_; }

    virtual Text to_text() const override { return text_; }
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