#pragma once

#include "Arduino.h"

#include <FastGPIO.h>
#define USE_FAST_GPIO

typedef unsigned long Micros;

typedef uint8_t PinValue;
typedef int16_t StepInt;
typedef int8_t SpeedInRevsPerMinuteInt;

constexpr int16_t pulse_time = 40;

template <uint8_t stepPin, uint8_t dirPin, uint8_t centerPin>
class Stepper final
{
  const StepInt number_of_steps;
  StepInt step_number = 0; // which step the motor is on

private:
public:
  bool speed_up = false;


  typedef int32_t TrackTime;
  TrackTime last_step_time = 0;  // time stamp in micros of when the last step was taken
  TrackTime first_step_time = 0; // first stamp in micros of when the first steps was taken
  TrackTime next_step_time = 0;

  bool ghost = false;
  volatile bool defecting = false;
  int defecting_delay = 0;
  StepInt offset_steps_{0};
  int8_t direction = 0;
  int16_t step_delay = 100; // delay between steps, in micros, based on speed
  int16_t step_on_fail_delay = 80;
  int16_t step_current = 0;
  bool pulsing = false; // currently pulsing
  bool behind = false;
  SpeedInRevsPerMinuteInt speed_in_revs_per_minute = 1;

  inline bool get_magnet_pin() const __attribute__((always_inline))
  {
#ifdef USE_FAST_GPIO
    return !FastGPIO::Pin<centerPin>::isInputHigh();
#else
    return digitalRead(centerPin) == LOW;
#endif
  }

  inline void set_step_pin(PinValue value) __attribute__((always_inline))
  {
#ifdef USE_FAST_GPIO
    FastGPIO::Pin<stepPin>::setOutputValue(value);
#else
    digitalWrite(stepPin, value);
#endif
  }

  inline void set_direction_pin(PinValue value) __attribute__((always_inline))
  {
#ifdef USE_FAST_GPIO
    FastGPIO::Pin<dirPin>::setOutputValue(value);
#else
    digitalWrite(dirPin, value);
#endif
  }

  inline int8_t asDirection(StepInt steps) __attribute__((always_inline))
  {
    return steps >= 0 ? 0 : 1;
  }

public:
  uint8_t turn_speed_in_revs_per_minute = 4;
  Stepper(StepInt number_of_steps) : number_of_steps(number_of_steps)
  {
  }

  StepInt get_offset_steps() const
  {
    return offset_steps_;
  }

  bool set_offset_steps(StepInt value)
  {
    if (offset_steps_ == value)
      return false;
    offset_steps_ = value;
    return true;
  }

  inline StepInt range(StepInt v) const __attribute__((always_inline))
  {
    while (v < 0)
    {
      v += number_of_steps;
    }
    while (v >= number_of_steps)
    {
      v -= number_of_steps;
    }
    return v;
  }

  inline StepInt rangeNew(StepInt v) const __attribute__((always_inline))
  {
    if (v < 0)
      return v + number_of_steps;
    if (v >= number_of_steps)
      return v - number_of_steps;
    return v;
  }
  void setGhosting(bool _ghost) { ghost = _ghost; }

  inline StepInt ticks() const __attribute__((always_inline)) { return range(step_number - offset_steps_); }

public:
  inline void enable_defecting(int expected_speed) __attribute__((always_inline))
  {
    defecting = true;
    defecting_delay = calculate_step_delay(expected_speed) - pulse_time;
  }
  inline void disable_defecting() __attribute__((always_inline))
  {
    defecting = false;
  }

  bool is_magnet_tick()
  {
    auto isCenterPinLow = get_magnet_pin();

    if (isCenterPinLow)
    {
      if (pulsing)
      {
        // reset as well
        pulsing = false;
        last_step_time = 0;
        set_step_pin(LOW);
      }
      step_number = 0;
      return true;
    }
    tryToStep(::micros());
    return false;
  }

  void updateDirection(int8_t new_direction)
  {
    if (this->direction == new_direction)
    {
      return;
    }
    this->direction = new_direction;
    if (speed_up)
      this->step_current = calculate_step_delay(turn_speed_in_revs_per_minute);
    else
      this->step_current = step_delay;
    set_direction_pin(direction == 0 ? LOW : HIGH);
  }

public:
  void setup()
  {
    pinMode(dirPin, OUTPUT);
    pinMode(stepPin, OUTPUT);
    pinMode(centerPin, INPUT);

    digitalWrite(dirPin, LOW);
    digitalWrite(stepPin, LOW);
  }

  inline int getStepNumber() const
  {
    return step_number;
  }

  static inline int16_t join(const int16_t current, const int16_t goal, const float multiplier) __attribute__((always_inline))
  {
    if (goal == current)
      return goal;
    auto inc = goal - current;
    //if (abs(inc) < 100)
    //  return goal;
    auto ret = current + (inc * multiplier);
    return ret;
  };

  inline bool is_behind() const __attribute__((always_inline))
  {
    return last_step_time - next_step_time > 100;
  }

  bool tryToStep(Micros now)
  {
    if (this->last_step_time == 0)
    {
      this->last_step_time = now;
      this->first_step_time = now;
      this->next_step_time = now + step_delay;
      this->behind = false;
      return false;
    }

    const int16_t diff = now - this->last_step_time;
    if (pulsing)
    {
      // pulsing can be very short
      //bool too_fast = last_step_time < next_step_time;
      if (diff < (behind ? pulse_time >> 1 : pulse_time))
      {
        return false;
      }
      this->pulsing = false;
      if (!ghost)
        set_step_pin(LOW);

      this->last_step_time = now;
      next_step_time += pulse_time;
      return false;
    }

    if (diff < (speed_up ? step_current : step_delay))
    {
      // we need to wait
      return false;
    }
    this->last_step_time = now;
    behind = is_behind(); // note this one should be really in between!!
    next_step_time += defecting ? defecting_delay : step_delay;
    pulsing = true;

    if (speed_up)
    {
      // previous was faster
      //bool too_fast = last_step_time < next_step_time;
      if (step_current < step_on_fail_delay
          // previus was slower
          || step_current > step_delay)
      {
        step_current = join(step_current, behind ? step_on_fail_delay : step_delay, .3); //step_on_fail_delay;
      }
      else if (behind)
      {
        // we are going too fast..., let slow down
        step_current = join(step_current, step_on_fail_delay, .3);
      }
      else
      {
        // we are going too slow...
        step_current = join(step_current, step_delay, .6);
      }
    }
    else
    {
      step_current = step_delay;
    }

    if (ghost)
    {
      return true;
    }
    set_step_pin(HIGH);
    if (this->direction == 0)
    {
      ++this->step_number;
      if (this->step_number == this->number_of_steps)
        this->step_number = 0;
    }
    else
    {
      --this->step_number;
      if (this->step_number == -1)
      {
        this->step_number += this->number_of_steps;
      }
    }
    return true;
  }

  void step(StepInt steps)
  {
    if (this->step_delay == 0)
    {
      // you could wait for ever :S
      return;
    }
    updateDirection(asDirection(steps));
    steps = abs(steps);
    while (steps > 0)
    {
      if (tryToStep(::micros()))
      {
        --steps;
      }
    }
  }

  void sync()
  {
    this->defecting = false;
    this->step_current = 0;
    this->last_step_time = 0;
    this->first_step_time = 0;
    this->next_step_time = 0;
  }

  int16_t calculate_speed_in_revs_per_minute(const int16_t delay) const
  {
    return abs(60.0 * 1000.0 * 1000L / this->number_of_steps / delay);
  }

  int16_t calculate_step_delay(const int16_t speed_in_revs_per_minute) const
  {
    if (speed_in_revs_per_minute == 0)
      return pulse_time;

    int ret = abs(60.0 * 1000.0 * 1000L / this->number_of_steps / speed_in_revs_per_minute);
    if (ret < pulse_time)
    {
      ESP_LOGE(TAG, "TOO FAST !!! speed_in_revs_per_minute=%d", speed_in_revs_per_minute);
      return pulse_time;
    }
    return ret;
  }

  /**
       Returns the delay in microseconds
    */
  void set_speed_in_revs_per_minute(const SpeedInRevsPerMinuteInt value, bool reset_current = false)
  {
    this->speed_in_revs_per_minute = value;
    this->step_delay = calculate_step_delay(value) - pulse_time;
    this->step_on_fail_delay = max(calculate_step_delay(value * 2.5), 60) - pulse_time;
    updateDirection(asDirection(speed_in_revs_per_minute));
    if (reset_current)
    {
      this->step_current = calculate_step_delay(turn_speed_in_revs_per_minute) - pulse_time;
    }
  }

  void set_current_speed_in_revs_per_minute(const SpeedInRevsPerMinuteInt value)
  {
    this->speed_in_revs_per_minute = calculate_step_delay(value);
  }

  void dump_config(const __FlashStringHelper *tag)
  {
    ESP_LOGI(tag, "  T0=%d revs=%d",
             (int)get_offset_steps(), (int)speed_in_revs_per_minute);

    ESP_LOGI(tag, "T  paper=%ldms vs/- real=%ldms = %dms",
             long(next_step_time - first_step_time) / 1000, long(last_step_time - first_step_time) / 1000, long(next_step_time - last_step_time) / 1000);
    ESP_LOGI(tag, "   donf=%d<dc=%d<d=%d",
             (int)step_on_fail_delay, (int)step_current, (int)step_delay);
  }
};

#include "pins.h"

typedef Stepper<MOTOR_A_STEP, MOTOR_A_DIR, SLAVE_POS_B> Stepper0;
typedef Stepper<MOTOR_B_STEP, MOTOR_B_DIR, SLAVE_POS_A> Stepper1;

extern Stepper0 stepper0;
extern Stepper1 stepper1;
