#pragma once

#ifdef AVR
#include <FastGPIO.h>
#define USE_FAST_GPIO
#endif

typedef unsigned long Micros;

class Stepper
{
protected:
  const int number_of_steps;
  int step_number = 0; // which step the motor is on
  bool ghost = false;

public:
  int offset_steps;
  Stepper(int number_of_steps, int offset_steps) : 
    number_of_steps(number_of_steps), 
    offset_steps(offset_steps) 
    {}

  int range(int v) const
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
  void setGhosting(boolean _ghost) { ghost = _ghost; }

  int ticks() const { return range(step_number - offset_steps); }

  virtual bool tryToStep(Micros now) = 0;
  virtual int setSpeed(int speedInRevsPerMinute) = 0;
  virtual int findZero() = 0;
};

template <uint8_t stepPin, uint8_t dirPin, uint8_t centerPin>
class StepperImpl : public Stepper
{
  const int pulseTime = 200;
  int direction = 0;
  Micros step_delay = 100;   // delay between steps, in micros, based on speed
  Micros last_step_time = 0; // time stamp in micros of when the last step was taken
  bool pulsing = false;      // currently pulsing
  int speedInRevsPerMinute = 1;

public:
  StepperImpl(int number_of_steps, int offset_steps)
      : Stepper(number_of_steps, offset_steps)
  {
  }

  void calibrate()
  {
    step_number = number_of_steps / 2 + offset_steps;
  }

  int findZero() override
  {

#ifdef USE_FAST_GPIO
    auto isCenterPinLow= !FastGPIO::Pin<centerPin>::isInputHigh();
#else
    auto isCenterPinLow = digitalRead(centerPin) == LOW;
#endif
    if (isCenterPinLow)
    {
      if (pulsing)
      {
        // reset as well
        pulsing = false;
        last_step_time = 0;
#ifdef USE_FAST_GPIO
        FastGPIO::Pin<stepPin>::setOutputValue(LOW);
#else
        digitalWrite(stepPin, LOW);
#endif
      }
      step_number = 0;
      return 0;
    }
    tryToStep(micros());
    return 1;
  }

  int asDirection(int steps)
  {
    return steps >= 0 ? 0 : 1;
  }

  void updateDirection(int new_direction)
  {
    if (this->direction == new_direction)
    {
      return;
    }
    this->direction = new_direction;
#ifdef USE_FAST_GPIO
    FastGPIO::Pin<dirPin>::setOutputValue(direction == 0 ? LOW : HIGH);
#else
    digitalWrite(dirPin, direction == 0 ? LOW : HIGH);
#endif
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

  int getSpeedInRevsPerMinute() const
  {
    return speedInRevsPerMinute;
  }

  int getStepNumber() const
  {
    return step_number;
  }

  bool tryToStep(Micros now) override
  {
    boolean step = now - this->last_step_time >= (pulsing ? pulseTime : this->step_delay);
    if (!step)
      return false;

    if (pulsing)
    {
      this->pulsing = false;
#ifdef USE_FAST_GPIO
      FastGPIO::Pin<stepPin>::setOutputValue(LOW);
#else
      digitalWrite(stepPin, LOW);
#endif
      return false;
    }
    if (this->step_delay == 0)
    {
      return false;
    }
    // get the timeStamp of when you stepped:
    this->last_step_time = now;
    pulsing = true;

    if (ghost)
    {
      return true;
    }
#ifdef USE_FAST_GPIO
    FastGPIO::Pin<stepPin>::setOutputValue(HIGH);
#else
    digitalWrite(stepPin, HIGH);
#endif
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

  void step(int steps)
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
      if (tryToStep(micros()))
      {
        --steps;
      }
    }
  }
  /**
       Returns the delay in microseconds
    */
  int setSpeed(int speedInRevsPerMinute) override
  {
    this->speedInRevsPerMinute = speedInRevsPerMinute;
    if (speedInRevsPerMinute == 0)
    {
      this->step_delay = 0;
      updateDirection(0);
    }
    else
    {
      this->step_delay = abs(60L * 1000L * 1000L / this->number_of_steps / speedInRevsPerMinute);
      updateDirection(asDirection(speedInRevsPerMinute));
    }
    return this->step_delay;
  }
};
