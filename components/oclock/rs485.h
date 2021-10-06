#pragma once

#include "Arduino.h"


class RS485Protocol;

class RS485
{
private:
  bool receiving = false;
  const long speed{115200};
  const int de_pin_;
  const int re_pin_;
  RS485Protocol *const protocol_;
  void startTransmitting();

protected:
  void _send(const byte *bytes, const byte length);
  template <class M>
  void _send(const M &m) { _send((const byte *)&m, (byte)sizeof(M)); }

protected:
    virtual void process(const byte *bytes, const byte length) = 0;

public:
  RS485(int de_pin, int re_pin);
  void dump_config();

  void setup();
  void loop();

  void startReceiving();
  
  template <class M>
  void send(const M &m) { _send(m); }
};