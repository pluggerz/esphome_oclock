#pragma once

#include "oclock.h"

#include <Arduino.h>

#ifdef MASTER_MODE
#include <iostream>
#include <math.h>
#endif


enum CmdEnum
{
  NONE = 0,
  GHOST = 0x1,
  CLOCK_WISE = 0x2,       // otherwise ANTI_CLOCK_WISE
  ANTI_CLOCK_WISE = NONE, // dont use for testing!
  ABSOLUTE = 0x4,         // otherwise RELATIVE
  RELATIVE = NONE,        // dont use for testing!
};

typedef uint16_t CmdInt;
// mode ->  total: 3 bits
#define MODE_WIDTH 3
#define MODE_SHIFT 0
// speed -> total: 3 + 3 = 6 bits
#define SPEED_WIDTH 3 // speed 0=>1, 1=2, 2=>4, 3=8, 4=16, 5=32
#define SPEED_SHIFT MODE_WIDTH
// steps -> total: 6 + 10 = 16 bits
#define STEPS_WIDTH 10
#define STEPS_SHIFT (SPEED_SHIFT + SPEED_WIDTH)

#define STEPS_MASK ((1 << STEPS_WIDTH) - 1)
#define MODE_MASK ((1 << MODE_WIDTH) - 1)
#define SPEED_MASK ((1 << SPEED_WIDTH) - 1)

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
  Cmd(CmdEnum mode, u16 steps, u8 speed) : mode((uint8_t)mode), speed(speed), steps(steps)
  {
  }
  Cmd(uint8_t mode, u16 steps, u8 speed) : mode(mode), speed(speed), steps(steps)
  {
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
    return Cmd(CmdEnum::CLOCK_WISE, discrete ? CmdSpecialMode::FOLLOW_SECONDS_DISCRETE : CmdSpecialMode::FOLLOW_SECONDS, (u8)speed);
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
  uint8_t dstId;

public:
  int getSrcId() const { return srcId; }
  MsgType getMsgType() const { return (MsgType)msgType; }
  int getDstId() const { return dstId; }

  UartMessage(uint8_t srcId, MsgType msgType) : srcId(srcId), msgType((u8)msgType), dstId(ALL_SLAVES) {}
  UartMessage(uint8_t srcId, MsgType msgType, uint8_t dstId) : srcId(srcId), msgType((uint8_t)msgType), dstId(dstId) {}

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

#define MAX_ANIMATION_KEYS 40
#define MAX_ANIMATION_KEYS_PER_MESSAGE 14 // MAX 14!

struct UartKeysMessage : public UartMessage
{
  // it would be tempting to use: Cmd cmds[MAX_ANIMATION_KEYS_PER_MESSAGE] ={}; but padding is messed up between esp and uno :()
  // note we control the bits better ;P
private:
  u8 _size;
  CmdInt cmds[MAX_ANIMATION_KEYS_PER_MESSAGE] = {};

public:
  UartKeysMessage(u8 dstId, u8 _size) : UartMessage(-1, MSG_SEND_KEYS, dstId), _size(_size)
  {
  }
  u8 size() const
  {
    return _size;
  }

  CmdInt operator[](int idx) const
  {
    return cmds[idx];
  }

#ifdef MASTER_MODE
  void setCmd(int idx, const Cmd &cmd)
  {
    cmds[idx] = cmd.asRaw();
  }
#endif
  Cmd getCmd(int idx) const
  {
    return Cmd(cmds[idx]);
  }
} __attribute__((packed, aligned(1)));

struct UartPosRequest : public UartMessage
{
private:
  uint16_t pos1;
  uint16_t pos2;

public:
  uint16_t getPos1() const { return pos1; }
  uint16_t getPos2() const { return pos2; }

  UartPosRequest() : UartMessage(-1, MSG_POS_REQUEST, 0), pos1(0), pos2(0) {}
  UartPosRequest(u8 srcId, u8 dstId, uint16_t pos1, uint16_t pos2) : UartMessage(srcId, MSG_POS_REQUEST, dstId), pos1(pos1), pos2(pos2) {}
} __attribute__((packed, aligned(1)));

/***
 * 
 * Essentially every minute we send animation keys.
 * Some type of animations depends on the seconds.
 * Ideally, a minute has 60 seconds, but some time could be lost in prepping.
 * So, this message will give the actual minute. 
 * Or at least the time when the master will probability send the next instructions.
 */
struct UartEndKeysMessage : public UartMessage
{
public:
  u16 numberOfMillisLeft;

  UartEndKeysMessage(u16 numberOfMillisLeft) : UartMessage(-1, MSG_END_KEYS, ALL_SLAVES), numberOfMillisLeft(numberOfMillisLeft)
  {
  }
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
    return as<UartMessage>()->getSrcId();
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

class UART 
{
  bool receiving = false;
  const long speed{115200};
  const int de_pin_;
  const int re_pin_;

private:
  void send(const byte *bytes, const byte length);

public:
  void setup();
  void dump_config();
  void loop(Micros now);

public:
  UART(int de_pin, int re_pin) : de_pin_(de_pin), re_pin_(re_pin) {}
  void startTransmitting();

#ifdef MASTER_MODE
  String asString(const UartMessage &m) const
  {
    return String("send: ") + "srcId[" + m.getSrcId() + "] msgType[" + m.getMsgType() + "] dstId[" + m.getDstId() + "]";
  }
  String asString(const UartAcceptMessage &m) const
  {
    return String("send: ") + "srcId[" + m.getSrcId() + "] msgType[" + m.getMsgType() + "] dstId[" + m.getDstId() + "] assignedId[" + m.getAssignedId() + "]";
  }
#endif

  template <typename M>
  void send(const M &m)
  {
#ifdef MASTER_MODE
    // Logger::info(String("Send: ") + asString(m));
#endif
    send((const byte *)&m, (byte)sizeof(M));
  }

  void startReceiving();

  void process(const UartMessageHolder &holder);
};

