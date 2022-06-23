#pragma once
#include "oclock.h"
#include "Arduino.h"

class Buffer
{
private:
  byte *bytesPtr_;
  uint8_t size_;

public:
  uint8_t size() const { return size_; }
  Buffer(byte *bytes_ptr, uint8_t size) : bytesPtr_(bytes_ptr), size_(size){};
  // TODO: get rid of raw
  byte *raw() const { return bytesPtr_; }
};

class Gate
{
public:
  virtual void dump_config(const char *tag) = 0;

  virtual void setup() = 0;
  virtual void start_receiving() = 0;
  virtual void start_transmitting() = 0;
};

#ifdef AVS
#include <FastGPIO.h>
#define USE_FAST_GPIO
#endif

template <uint8_t de_pin_, uint8_t re_pin_>
class RS485Gate : public Gate
{
  void dump_config(const char *tag)
  {
    ESP_LOGI(tag, "          de_pin: %d", de_pin_);
    ESP_LOGI(tag, "          re_pin: %d", re_pin_);
  }

  void setup() override
  {
    pinMode(de_pin_, OUTPUT);
    pinMode(re_pin_, OUTPUT);

#ifdef USE_FAST_GPIO
    ESP_LOGI(TAG, "RS485Gate(de_pin, re_pin, method)=(%d, %d, FastGpio)", de_pin_, re_pin_);
#else
    ESP_LOGI(TAG, "RS485Gate(de_pin, re_pin, method)=(%d, %d, ArduinoFramework)", de_pin_, re_pin_);
#endif
  }

  void start_receiving() override final
  {
#ifdef USE_FAST_GPIO
    FastGPIO::Pin<de_pin_>::setOutputValue(LOW);
    FastGPIO::Pin<re_pin_>::setOutputValue(LOW);
#else
    digitalWrite(de_pin_, LOW);
    digitalWrite(re_pin_, LOW);
#endif
  }
  void start_transmitting() override final
  {
#ifdef USE_FAST_GPIO
    FastGPIO::Pin<de_pin_>::setOutputValue(HIGH);
    FastGPIO::Pin<re_pin_>::setOutputValue(LOW);
#else
    digitalWrite(de_pin_, HIGH);
    digitalWrite(re_pin_, LOW);
#endif
  }
};

class Protocol;

class Channel
{
private:
  bool receiving = false;
  uint32_t baud_rate;
  Gate &gate;
  Protocol *const protocol_;
public: //NOTE: 'start_transmitting' should be private
  void start_transmitting();

protected:
  void _send(const byte *bytes, const byte length);
  template <class M>
  void _send(const M &m) { _send((const byte *)&m, (byte)sizeof(M)); }

protected:
  virtual void process(const byte *bytes, const byte length) = 0;

public:
  Channel(Gate &gate, Buffer &buffer);
  void dump_config(const char *tag);

  void setup();
  void loop();
  void skip();

  void start_receiving();

  void upgrade_baud_rate(uint32_t value);
  void downgrade_baud_rate();

  template <class M>
  void send(const M &m) { _send(m); }
};