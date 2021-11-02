#pragma once

/**
 * A tiny version of a sheduler 
 */

// make sure that it matches the the return of micros();
typedef unsigned long Micros;
typedef unsigned long Millis;
typedef unsigned long Units;

class Async
{
  unsigned char
     canceled : 1,
    initialized : 1;

protected:
  Async() : canceled(false), initialized(false)
  {
  }

public:
  virtual ~Async() {}
  bool isCanceled() const
  {
    return canceled;
  }
  bool isInitialized() const
  {
    return initialized;
  }

  /**
       Should be called from derived
    */
  /*
      Once an item is canceled it will be removed from the execution list
    */
  virtual void cancel()
  {
    canceled = true;
  }
  virtual void setup(Micros now)
  {
    initialized = true;
  };
  virtual void loop(Micros) {}
};

class AsyncDelay : virtual public Async
{
private:
  Micros t0_ = 0;
  typedef unsigned short DelayMicros;
  DelayMicros delay;

protected:
  virtual void step() = 0;

  void setDelay(DelayMicros delay) { this->delay = delay; }

public:
  AsyncDelay(DelayMicros delay) : delay(delay)
  {
  }

  void setup(Micros now) override
  {
    Async::setup(now);

    t0_ = now;
    return;
  }

  void loop(Micros t1) override final
  {
    if (t1 - t0_ > delay)
    {
      t0_ = t1;
      step();
    }
  }
};

enum TimeUnit
{
  SECONDS,
  MILLIS,
  MICROS
};

class ToMicros
{
public:
  static Micros toMicros(Units units, TimeUnit timeUnit)
  {
    switch (timeUnit)
    {
    case SECONDS:
      return units * 1000 * 1000;
    case MILLIS:
      return units * 1000;
    default:
      return units;
    }
  }
  static Micros fromSeconds(Units units)
  {
    return toMicros(units, TimeUnit::SECONDS);
  }
  static Micros fromMillis(Units units)
  {
    return toMicros(units, TimeUnit::MILLIS);
  }
};

/**
   Since we (mis)use the fact the the CPU is single threaded, therefore at any point in the code you can register an async activity

*/

class AsyncRegister
{
private:
  static void add(const char *id, Async *async);

public:
  static void anom(Async *async)
  {
    add(nullptr, async);
  }

  static void byName(const char *name, Async *async)
  {
    add(name, async);
  }

  static void remove(const char *name)
  {
    add(name, nullptr);
  }

  static void loop(Millis millis);
};