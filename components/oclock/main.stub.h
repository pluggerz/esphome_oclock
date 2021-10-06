#pragma once

#include "oclock.h"

#ifdef ESPHOME_MODE

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "master.h"

namespace esphome
{
    namespace oclock_stub
    {
        class OClockStubController : public Component
        {
        protected:
            const int countStart{-1};
            HighFrequencyLoopRequester high_freq_;

            long t0;       // 'start'
            long t1;       // 'track the seconds'
            long hits{0L}; // all hits
            bool loop_master_{false};
            long count_{0};
            bool stats{false};

        public:
            OClockStubController()
            {
                t1 = t0 = millis();
            }

            void setup()
            {
                ESP_LOGCONFIG(TAG, "OClockStub");
                // make sure we get all the attention!
                high_freq_.start();
                if (countStart == -1)
                {
                    oclock::master.setup();
                    loop_master_ = true;
                }
            }

            void loop()
            {
                ++hits;
                auto t = millis();
                if (t - t1 > 1000)
                {
                    auto diff = t - t0;
                    auto secs = ((double)diff / 1000.0);
                    auto perSec = (double)hits / secs;
                    if (stats)
                        ESP_LOGI(TAG, "OClockStubController::loop %f per sec (total: %f) %s", perSec, secs,
                                 HighFrequencyLoopRequester::is_high_frequency() ? "is_high_frequency" : "NOT is_high_frequency");
                    t1 = t;
                }
                if (t - t0 > 10000)
                {
                    hits = 0;
                    t0 = t1 = t;
                    if (stats)
                        ESP_LOGI(TAG, "OClockStubController::loop RESET stats");
                }

                if (loop_master_)
                    oclock::master.loop();
            }

            void set_time(esphome::time::RealTimeClock* time) { 
                oclock::master.set_time(time);
            }

            void dump_config()
            {
                count_++;
                ESP_LOGCONFIG(TAG, "STUB: count=%d (countStart=%d)", count_, countStart);

                if (count_ == countStart && loop_master_ == false)
                {
                    ESP_LOGCONFIG(TAG, "STUB: activating master!");
                    oclock::master.setup();
                    loop_master_ = true;
                }
                oclock::master.dump_config();
            }

        protected:
        };
    } // namespace oclock
} // namespace esphome
#endif