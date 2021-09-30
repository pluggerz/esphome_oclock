#pragma once

#include "Arduino.h"
#include "timing.h"

class RS485 {
  bool receiving = false;
  const long speed{115200};
  const int de_pin_;
  const int re_pin_;

private:
  void send(const byte *bytes, const byte length);

public:
  RS485(int de_pin, int re_pin) : de_pin_(de_pin), re_pin_(re_pin) {}
  void dump_config();

  void setup();
  void loop();

  void startReceiving();
  void startTransmitting();
 

  template <typename M>
  void send(const M &m)
  {
    send((const byte *)&m, (byte)sizeof(M));
  }
};