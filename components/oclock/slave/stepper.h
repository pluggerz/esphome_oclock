#pragma once

#include "Arduino.h"

#include <FastGPIO.h>
#define USE_FAST_GPIO

typedef unsigned long Micros;

typedef int PinValue;
typedef int16_t StepInt;
typedef int8_t SpeedInRevsPerMinuteInt;

template <uint8_t stepPin, uint8_t dirPin, uint8_t centerPin>
class Stepper final
{
protected:
  const StepInt number_of_steps;
  StepInt step_number = 0; // which step the motor is on
  bool ghost = false;
  StepInt offset_steps_{0};
  const Micros pulseTime = 5;
  int8_t direction = 0;
  Micros step_delay = 100;   // delay between steps, in micros, based on speed
  Micros last_step_time = 0; // time stamp in micros of when the last step was taken
  Micros real_step_time = 0;
  bool pulsing = false; // currently pulsing
  int8_t speed_in_revs_per_minute = 1;

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
  void setGhosting(bool _ghost) { ghost = _ghost; }

  inline StepInt ticks() const __attribute__((always_inline)) { return range(step_number - offset_steps_); }

public:
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

  bool tryToStep(Micros now)
  {
    auto real_diff = now - this->real_step_time;
    if (real_diff < (pulsing ? pulseTime : 40))
    {
      // we really need to wait :S
      return false;
    }

    auto diff = now - this->last_step_time;
    bool step = diff >= (pulsing ? pulseTime : this->step_delay - pulseTime);
    if (!step)
      return false;
    this->real_step_time = now;

    if (pulsing)
    {
      this->pulsing = false;
      set_step_pin(LOW);
      //this->last_step_time += pulseTime;
      return false;
    }

    if (this->step_delay == 0)
    {
      return false;
    }
    // we assuma that we 'rotate' for ever, or after a set_time
    if (this->last_step_time == 0)
    {
      this->last_step_time = now;
    }
    else
    {
      // It is temping to do: this->last_step_time = now, but that will add some loss time: now - last_step_time
      this->last_step_time += step_delay;
    }
    pulsing = true;

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
    auto now = ::micros();
    this->last_step_time = now;
    this->real_step_time = now;
  }

  /**
       Returns the delay in microseconds
    */
  int set_speed_in_revs_per_minute(SpeedInRevsPerMinuteInt value)
  {
    this->speed_in_revs_per_minute = value;
    if (speed_in_revs_per_minute == 0)
    {
      this->step_delay = 0;
      updateDirection(0);
    }
    else
    {
      this->step_delay = abs(60L * 1000L * 1000L / this->number_of_steps / speed_in_revs_per_minute);
      updateDirection(asDirection(speed_in_revs_per_minute));
    }
    return this->step_delay;
  }
};

#include "pins.h"

typedef Stepper<MOTOR_A_STEP, MOTOR_A_DIR, SLAVE_POS_B> Stepper0;
typedef Stepper<MOTOR_B_STEP, MOTOR_B_DIR, SLAVE_POS_A> Stepper1;
