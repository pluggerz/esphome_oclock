#pragma once

#ifdef FSfdsFADSfadsfds

#include "oclock.h"

#include <Arduino.h>


enum CmdSpecialMode
{
  FOLLOW_SECONDS_DISCRETE = ((1 << STEPS_WIDTH) - 1),
  FOLLOW_SECONDS = ((1 << STEPS_WIDTH) - 2)
};

struct Cmd
{
  uint8_t mode;
  uint8_t speed;
  uint16_t steps;

#ifdef MASTER_MODE
  // curious Arduino knows log but no log2 !?
  // compression: speed in [0..4) -> 0=compressed => 4=decompressed, [4..8) -> 1 => 8 [8..16) -> 2 => 16
  static int compressSpeed(int value)
  {

    value = value >> 1;
    if (value <= 0)
      return 0;
    int ret = log2((double)value);
    return ret >= SPEED_MASK ? SPEED_MASK : ret;
  }
#endif

  static int decompressSpeed(int value)
  {
    return 1 << (value + 2);
  }

public:
#ifdef MASTER_MODE
  CmdInt asRaw() const
  {
    return (static_cast<CmdInt>(mode & MODE_MASK) << MODE_SHIFT) |
           (static_cast<CmdInt>(Cmd::compressSpeed(speed) & SPEED_MASK) << SPEED_SHIFT) |
           (static_cast<CmdInt>(steps & STEPS_MASK) << STEPS_SHIFT);
  }
#endif
  Cmd() : mode(0), speed(0), steps(0)
  {
  }
  Cmd(CmdInt raw) : mode(static_cast<uint8_t>((raw >> MODE_SHIFT) & MODE_MASK)),
                    speed(Cmd::decompressSpeed(static_cast<CmdInt>((raw >> SPEED_SHIFT) & SPEED_MASK))),
                    steps(static_cast<uint16_t>((raw >> STEPS_SHIFT) & STEPS_MASK))
  {
  }
#ifdef MASTER_MODE
  static int fit_speed(int speed)
  {
    return Cmd::decompressSpeed(Cmd::compressSpeed(speed));
  }
  Cmd(CmdEnum mode, u16 steps, u8 speed) : mode((uint8_t)mode), speed(fit_speed(speed)), steps(steps)
  {
  }
  Cmd(uint8_t mode, u16 steps, u8 speed) : mode(mode), speed(fit_speed(speed)), steps(steps)
  {
  }

  Mllis timeInMillis()  {

  }

#endif
  bool isEmpty() const
  {
    return mode == 0 && speed == 0 && steps == 0;
  }
};

#ifdef MASTER_MODE
class SpecialCmds
{
public:
  static Cmd followSeconds(u8 speed, bool discrete)
  {
    return Cmd(CmdEnum::CLOCKWISE, discrete ? CmdSpecialMode::FOLLOW_SECONDS_DISCRETE : CmdSpecialMode::FOLLOW_SECONDS, (u8)speed);
  }
};

std::ostream &operator<<(std::ostream &out, const Cmd &c);
#endif

const int MAX_UART_MESSAGE_SIZE = 32;
const int ALL_SLAVES = 32;

enum MsgType
{
  MSG_ID_RESET = 0,
  MSG_ID_START = 1,
  MSG_ID_ACCEPT = 2,
  MSG_ID_DONE = 3,
  MSG_POS_REQUEST = 4,
  MSG_BEGIN_KEYS = 5,
  MSG_SEND_KEYS = 6,
  MSG_END_KEYS = 7,
  MSG_CALIBRATE_START = 8,
  MSG_CALIBRATE_END = 9,
  // MSG_LED = 10,
  MSG_COLOR = 11,
  MSG_COLOR_ANIMATION = 12,
};

#ifdef MASTER_MODE
std::ostream &operator<<(std::ostream &os, MsgType ethertype);
#endif

struct UartMessage
{
private:
  uint8_t srcId;
  uint8_t msgType;
  uint8_t destination_id;

public:
  int getSourceId() const { return srcId; }
  MsgType getMsgType() const { return (MsgType)msgType; }
  int getDstId() const { return destination_id; }

  UartMessage(uint8_t srcId, MsgType msgType) : srcId(srcId), msgType((u8)msgType), destination_id(ALL_SLAVES) {}
  UartMessage(uint8_t srcId, MsgType msgType, uint8_t destination_id) : srcId(srcId), msgType((uint8_t)msgType), destination_id(destination_id) {}

  MsgType getMessageType() const
  {
    return (MsgType)msgType;
  }
} __attribute__((packed, aligned(1)));

struct UartAcceptMessage : public UartMessage
{
private:
  uint8_t assignedId;

public:
  int getAssignedId() const { return assignedId; }

  UartAcceptMessage(uint8_t srcId, uint8_t assignedId) : UartMessage(srcId, MSG_ID_ACCEPT), assignedId(assignedId) {}
} __attribute__((packed, aligned(1)));

struct UartColorMessage : public UartMessage
{
private:
  uint8_t red, green, blue, brightness;

public:
  uint8_t getRed() const { return red; }
  uint8_t getGreen() const { return green; }
  uint8_t getBlue() const { return blue; }
  uint8_t getBrightness() const { return brightness; }

  UartColorMessage(uint8_t red, uint8_t green, uint8_t blue, uint8_t brightness)
      : UartMessage(-1, MSG_COLOR), red(red), green(green), blue(blue), brightness(brightness) {}
} __attribute__((packed, aligned(1)));

struct UartColorAnimationMessage : public UartMessage
{
private:
  uint8_t animation;
  uint8_t brightness;

public:
  uint8_t getAnimation() const { return animation; }
  uint8_t getBrightness() const { return brightness; }

  UartColorAnimationMessage(uint8_t animation, uint8_t brightness)
      : UartMessage(-1, MSG_COLOR_ANIMATION), animation(animation), brightness(brightness) {}
} __attribute__((packed, aligned(1)));

class UartMessageHolder
{
private:
  byte buffer[MAX_UART_MESSAGE_SIZE];
  //byte *buffer;

public:
  template <typename M>
  const M *as() const
  {
    return (const M *)buffer;
  }

  int getSourceId() const
  {
    return as<UartMessage>()->getSourceId();
  }

  int getDestinationId() const
  {
    return as<UartMessage>()->getDstId();
  }

  MsgType getMessageType() const
  {
    return as<UartMessage>()->getMessageType();
  }

  UartMessageHolder(const byte *_bytes, int length)
  {
    //buffer = (byte*)_bytes;
    memcpy(this->buffer, _bytes, length);
  }
};

#endif