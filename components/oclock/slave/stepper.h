#pragma once

#include "Arduino.h"

#include <FastGPIO.h>
#define USE_FAST_GPIO

typedef unsigned long Micros;

typedef uint8_t PinValue;
typedef int16_t StepInt;
typedef int8_t SpeedInRevsPerMinuteInt;

constexpr int16_t pulse_time = 40;
constexpr bool speed_up = false;

template <uint8_t stepPin, uint8_t dirPin, uint8_t centerPin>
class Stepper final
{
  const StepInt number_of_steps;
  StepInt step_number = 0; // which step the motor is on

private:
public:
  Micros last_step_time = 0;  // time stamp in micros of when the last step was taken
  Micros first_step_time = 0; // first stamp in micros of when the first steps was taken
  Micros next_step_time = 0;

  bool ghost = false;
  volatile bool defecting = false;
  int16_t defecting_crc{0};
  StepInt offset_steps_{0};
  int8_t direction = 0;
  int16_t step_delay = 100; // delay between steps, in micros, based on speed
  int16_t step_on_fail_delay = 80;
  int16_t step_current = 0;
  int16_t defect = 0;
  bool pulsing = false; // currently pulsing
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
  const uint8_t slowest_revs_per_minute = 4;
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
  inline void set_defecting(bool value) __attribute__((always_inline)) { defecting = value; }

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
      this->step_current = calculate_step_delay(slowest_revs_per_minute);
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

  static inline int join(const int current, const int goal, const int shift) __attribute__((always_inline))
  {
    if (goal == current)
      return goal;
    int inc = (goal - current) >> shift;
    int ret = current + inc;
    return ret;
  };

  bool tryToStep(Micros now)
  {
    if (this->last_step_time == 0 || this->first_step_time == 0)
    {
      this->last_step_time = now;
      this->first_step_time = now;
      this->next_step_time = now;
      return false;
    }

    const int16_t diff = now - this->last_step_time;
    if (pulsing)
    {
      // pulsing can be very short
      if (diff < (defect < 0 ? pulse_time : pulse_time >> 1))
      {
        return false;
      }
      this->pulsing = false;
      set_step_pin(LOW);

      defect += diff;
      if (!defecting)
      {
        defect -= pulse_time;
        next_step_time += pulse_time;
      }
      else
        defecting_crc += pulse_time;
      this->last_step_time = now;
      return false;
    }

    if (diff < (speed_up ? step_current : step_delay))
    {
      // we need to wait
      return false;
    }
    defect += diff;
    if (!defecting)
    {
      defect -= step_delay;
      next_step_time += step_delay;
    }
    else
      defecting_crc += step_delay;
    this->last_step_time = now;
    pulsing = true;

    if (speed_up)
    {
      // previous was faster
      bool too_fast = defect < -step_on_fail_delay;
      if (step_current < step_on_fail_delay
          // previus was slower
          || step_current > step_delay)
      {
        step_current = join(step_current, too_fast ? step_delay : step_on_fail_delay, 1); //step_on_fail_delay;
      }
      else if (too_fast)
      {
        // we are going too fast..., let slow down
        step_current = join(step_current, step_delay, 2);
      }
      else
      {
        // we are going too slow...
        step_current = join(step_current, step_on_fail_delay, 1);
      }
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
    this->defect = 0;
    this->step_current = 0;
    this->last_step_time = 0;
    this->first_step_time = 0;
    this-> next_step_time = 0;
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
  void set_speed_in_revs_per_minute(const SpeedInRevsPerMinuteInt value)
  {
    this->speed_in_revs_per_minute = value;
    this->step_delay = calculate_step_delay(value) - pulse_time;
    this->step_on_fail_delay = max(calculate_step_delay(value * 2), 80) - pulse_time;
    updateDirection(asDirection(speed_in_revs_per_minute));
  }

  void dump_config(const __FlashStringHelper *tag)
  {
    ESP_LOGI(tag, "  T0=%d revs=%d def=%dms",
             (int)get_offset_steps(), (int)speed_in_revs_per_minute, int(defect / 1000));
    ESP_LOGI(tag, "  %ld - %ld = %ld",
             long(next_step_time), long(first_step_time), long(next_step_time - first_step_time));
    ESP_LOGI(tag, "  %ld - %ld = %ld",
             long(last_step_time), long(first_step_time), long(last_step_time - first_step_time));
    ESP_LOGI(tag, "   donf=%d<dc=%d<d=%d",
             (int)step_on_fail_delay, (int)step_current, (int)step_delay);
  }
};

#include "pins.h"

typedef Stepper<MOTOR_A_STEP, MOTOR_A_DIR, SLAVE_POS_B> Stepper0;
typedef Stepper<MOTOR_B_STEP, MOTOR_B_DIR, SLAVE_POS_A> Stepper1;

extern Stepper0 stepper0;
extern Stepper1 stepper1;
