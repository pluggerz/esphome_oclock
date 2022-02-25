#pragma once

#include "oclock.h"
#include "channel.h"
#include "ticks.h"
#include "enums.h"

#ifdef ESP8266
#include <functional>
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
  MSG_LOG = 10,
  MSG_COLOR = 11,
  MSG_COLOR_ANIMATION = 12,
  MSG_LED_MODE = 13,
  MSG_DUMP_LOG_REQUEST = 14,
  MSG_SLAVE_CONFIG = 15,
  MSG_BRIGHTNESS = 16,
  MSG_BOOL_DEBUG_LED_LAYER = 17,
  MSG_INFORM_STOP_ANIMATION = 19,
  MSG_WAIT_FOR_ANIMATION = 20,
  MSG_FOREGROUND_RGB_LEDS = 21,
  MSG_BACKGROUND_RGB_LEDS = 22,
};

struct UartMessage
{
private:
  uint8_t source_id;
  uint8_t msgType;
  uint8_t destination_id;

public:
  int getSourceId() const { return source_id; }
  MsgType getMsgType() const { return (MsgType)msgType; }
  int getDstId() const { return destination_id; }

  UartMessage(uint8_t source_id, MsgType msgType) : source_id(source_id), msgType((u8)msgType), destination_id(ALL_SLAVES) {}
  UartMessage(uint8_t source_id, MsgType msgType, uint8_t destination_id) : source_id(source_id), msgType((uint8_t)msgType), destination_id(destination_id) {}

  MsgType getMessageType() const
  {
    return (MsgType)msgType;
  }
} __attribute__((packed, aligned(1)));

struct UartBoolMessage : public UartMessage
{
public:
  const bool value;

  UartBoolMessage(MsgType msgType, bool value) : UartMessage(-1, msgType), value(value) {}
} __attribute__((packed, aligned(1)));

struct UartRgbForegroundLedsMessage : public UartMessage
{
public:
  oclock::RgbColorLeds leds;
  UartRgbForegroundLedsMessage(const oclock::RgbColorLeds &leds) : UartMessage(-1, MSG_FOREGROUND_RGB_LEDS), leds(leds) {}
} __attribute__((packed, aligned(1)));

struct UartRgbBackgroundLedsMessage : public UartMessage
{
public:
  oclock::RgbColorLeds leds;
  UartRgbBackgroundLedsMessage(const oclock::RgbColorLeds &leds) : UartMessage(-1, MSG_BACKGROUND_RGB_LEDS), leds(leds) {}
} __attribute__((packed, aligned(1)));

struct UartAcceptMessage : public UartMessage
{
private:
  uint8_t assignedId;

public:
  int getAssignedId() const { return assignedId; }
  UartAcceptMessage(uint8_t source_id, uint8_t assignedId) : UartMessage(source_id, MSG_ID_ACCEPT), assignedId(assignedId) {}
} __attribute__((packed, aligned(1)));

struct UartSlaveConfigRequest : public UartMessage
{
public:
  int16_t handle_offset0;
  int16_t handle_offset1;
  int16_t initial_ticks0;
  int16_t initial_ticks1;

public:
  UartSlaveConfigRequest(uint8_t destination_id,
                         int16_t handle_offset0, int16_t handle_offset1,
                         int16_t initial_ticks0, int16_t initial_ticks1)
      : UartMessage(-1, MSG_SLAVE_CONFIG, destination_id),
        handle_offset0(handle_offset0), handle_offset1(handle_offset1),
        initial_ticks0(initial_ticks0), initial_ticks1(initial_ticks1)
  {
  }
} __attribute__((packed, aligned(1)));

struct UartDoneMessage : public UartMessage
{
public:
  uint8_t assignedId;
  uint32_t baud_rate;

public:
  UartDoneMessage(uint8_t source_id, uint8_t assignedId, uint32_t baud_rate) : UartMessage(source_id, MSG_ID_DONE), assignedId(assignedId), baud_rate(baud_rate) {}
} __attribute__((packed, aligned(1)));

struct LedModeRequest : public UartMessage
{
  uint8_t raw_mode_;

public:
  bool foreground;
  oclock::ForegroundEnum get_foreground_enum() const { return static_cast<oclock::ForegroundEnum>(raw_mode_); }
  oclock::BackgroundEnum get_background_enum() const { return static_cast<oclock::BackgroundEnum>(raw_mode_); }
  LedModeRequest(oclock::BackgroundEnum mode) : UartMessage(-1, MSG_LED_MODE), raw_mode_(int(mode)), foreground(false) {}
  LedModeRequest(oclock::ForegroundEnum mode) : UartMessage(-1, MSG_LED_MODE), raw_mode_(int(mode)), foreground(true) {}
} __attribute__((packed, aligned(1)));

struct UartLogMessage : public UartMessage
{
public:
  char buffer[24];
  static uint8_t length() { return 24; }
  uint8_t part;
  uint8_t total_parts;
  bool overflow;

  UartLogMessage(uint8_t source_id, uint8_t part, uint8_t total_parts, bool overflow) : UartMessage(source_id, MSG_LOG, -1), part(part), total_parts(total_parts), overflow(overflow) { buffer[0] = 0; }
} __attribute__((packed, aligned(1)));

struct UartPosRequest : public UartMessage
{
private:
public:
  bool stop;

  bool initialized;
  uint16_t pos0;
  uint16_t pos1;

  UartPosRequest(bool stop) : UartMessage(-1, MSG_POS_REQUEST, 0), stop(stop), initialized(true), pos0(0), pos1(0) {}
  UartPosRequest(bool stop, u8 source_id, u8 destination_id, uint16_t pos0, uint16_t pos1, bool initialized) : UartMessage(source_id, MSG_POS_REQUEST, destination_id), stop(stop), initialized(initialized), pos0(pos0), pos1(pos1) {}
} __attribute__((packed, aligned(1)));

struct UartDumpLogsRequest : public UartMessage
{
public:
  bool dump_config;
  UartDumpLogsRequest(bool dump_config) : UartMessage(-1, MSG_DUMP_LOG_REQUEST, 0), dump_config(dump_config) {}
  UartDumpLogsRequest(bool dump_config, u8 source_id, u8 destination_id) : UartMessage(source_id, MSG_DUMP_LOG_REQUEST, destination_id), dump_config(dump_config) {}
} __attribute__((packed, aligned(1)));

struct UartInformToStopAnimationRequest : public UartMessage
{
public:
  UartInformToStopAnimationRequest() : UartMessage(-1, MSG_INFORM_STOP_ANIMATION) {}
} __attribute__((packed, aligned(1)));

struct UartWaitUntilAnimationIsDoneRequest : public UartMessage
{
public:
  UartWaitUntilAnimationIsDoneRequest() : UartMessage(-1, MSG_WAIT_FOR_ANIMATION, 0) {}
  UartWaitUntilAnimationIsDoneRequest(u8 source_id, u8 destination_id) : UartMessage(source_id, MSG_WAIT_FOR_ANIMATION, destination_id) {}
} __attribute__((packed, aligned(1)));

struct UartScaledBrightnessMessage : public UartMessage
{
public:
  // value between 0..5
  const uint8_t scaled_brightness;

  UartScaledBrightnessMessage(uint8_t scaled_brightness) : UartMessage(-1, MSG_BRIGHTNESS), scaled_brightness(scaled_brightness) {}
} __attribute__((packed, aligned(1)));

struct UartColorMessage : public UartMessage
{
public:
  oclock::RgbColor color;

  UartColorMessage(const oclock::RgbColor &c) : UartMessage(-1, MSG_COLOR), color(c) {}
} __attribute__((packed, aligned(1)));

class InteropStringifier
{
public:
  static const __FlashStringHelper *asF(const MsgType &type)
  {
    switch (type)
    {
    case MSG_ID_RESET:
      return F("ID_RESET");
    case MSG_ID_START:
      return F("ID_START");
    case MSG_ID_ACCEPT:
      return F("ID_ACCEPT");
    case MSG_ID_DONE:
      return F("ID_DONE");
    case MSG_POS_REQUEST:
      return F("POS");
    case MSG_BEGIN_KEYS:
      return F("BEGIN_KEYS");
    case MSG_SEND_KEYS:
      return F("SEND_KEYS");
    case MSG_END_KEYS:
      return F("END_KEYS");
    case MSG_CALIBRATE_START:
      return F("CALIBRATE_START");
    case MSG_CALIBRATE_END:
      return F("CALIBRATE_END");
    case MSG_COLOR:
      return F("COLOR");
    case MSG_COLOR_ANIMATION:
      return F("COLOR_ANIMATION");
    case MSG_LOG:
      return F("LOG");
    case MSG_LED_MODE:
      return F("LED_MODE");
    case MSG_DUMP_LOG_REQUEST:
      return F("DUMP_LOGS");
    case MSG_SLAVE_CONFIG:
      return F("CONFIG");
    case MSG_INFORM_STOP_ANIMATION:
      return F("INFORM_STOP_ANIMATION");
    case MSG_WAIT_FOR_ANIMATION:
      return F("WAIT_FOR_ANIMATION");
    default:
      return F("MSG_???");
    }
  }

  static const __FlashStringHelper *asIdF(int id)
  {
    if (id == ALL_SLAVES)
      return F("*");
    if (id == 0xFF)
      return F("M");

#define CASE(X) \
  case X:       \
    return F("S" #X "");

    switch (id >> 1)
    {
      CASE(0)
      CASE(1)
      CASE(2)
      CASE(3)
      CASE(4)
      CASE(5)
      CASE(6)
      CASE(7)
      CASE(8)
      CASE(9)
      CASE(10)
      CASE(11)
      CASE(12)
      CASE(13)
      CASE(14)
      CASE(15)
      CASE(16)
      CASE(17)
      CASE(18)
      CASE(19)
      CASE(20)
      CASE(21)
      CASE(22)
      CASE(23)

    default:
      return F("S?");
    }
#undef CASE
  }

#ifdef AVR
#define LOG_MESSAGED(what, msg, length)
#else
#define LOG_MESSAGED(what, msg, length) InteropStringifier::logMessage(ESPHOME_LOG_LEVEL_DEBUG, TAG, __LINE__, F(what), msg, length)
#endif
#define LOG_MESSAGEI(what, msg, length) InteropStringifier::logMessage(ESPHOME_LOG_LEVEL_INFO, TAG, __LINE__, F(what), msg, length)
#define LOG_MESSAGEW(what, msg, length) InteropStringifier::logMessage(ESPHOME_LOG_LEVEL_WARN, TAG, __LINE__, F(what), msg, length)

  static void logMessage(int level, const char *tag, int line, const __FlashStringHelper *pre, const UartMessage *msg, int length)
  {
    switch (msg->getMsgType())
    {
#define DEF_PRINT(STR, ...)                                      \
  esp_log_printf_(level, tag, line, F("%S[%d]: [%S>%S@%S]" STR), \
                  pre,                                           \
                  length,                                        \
                  asIdF(msg->getSourceId()),                     \
                  asF(msg->getMsgType()),                        \
                  asIdF(msg->getDstId()),                        \
                  __VA_ARGS__)

    case MSG_ID_ACCEPT:
      DEF_PRINT(">%S", asIdF(reinterpret_cast<const UartAcceptMessage *>(msg)->getAssignedId()));
      break;

    case MSG_ID_DONE:
      DEF_PRINT(">%S", asIdF(reinterpret_cast<const UartDoneMessage *>(msg)->assignedId));
      break;

    case MSG_POS_REQUEST:
    {
      auto posMsg = reinterpret_cast<const UartPosRequest *>(msg);
      DEF_PRINT(">(%d, %d)", posMsg->pos0, posMsg->pos1);
    }
    break;

    case MSG_LOG:
    {
      auto logMsg = reinterpret_cast<const UartLogMessage *>(msg);
      DEF_PRINT(" part[%d/%d]", logMsg->part, logMsg->total_parts);
    }
    break;

    case MSG_SLAVE_CONFIG:
    {
      auto configMsg = reinterpret_cast<const UartSlaveConfigRequest *>(msg);
      DEF_PRINT(" handle_offset(%d, %d) initial_ticks(%d, %d)",
                configMsg->handle_offset0, configMsg->handle_offset1,
                configMsg->initial_ticks0, configMsg->initial_ticks1);
    }
    break;

    default:
      DEF_PRINT("", nullptr);
      break;
    }
#undef DEF_PRINT
  }
};

#ifdef ESP8266
typedef std::function<bool(const UartMessage *msg)> InteropRS485ReceiverFuncPtr;
#else
typedef bool (*InteropRS485ReceiverFuncPtr)(const UartMessage *msg);
#endif

class InteropRS485 : public Channel
{
private:
  /**
   * owner_id:
   * if -1/0xFF accept all messages: NB: filtering should be done by the listener ( master )
   * if ALL_SLAVES broadcast address
   * otherwise
   */
  uint8_t owner_id_{ALL_SLAVES};
  InteropRS485ReceiverFuncPtr listener_{nullptr};

  inline bool for_me(int destination_id) const
  {
    // master listen to all
    if (owner_id_ == 0xFF)
      return true;
    // in most cases
    if (destination_id == owner_id_)
      return true;
    // broad cast?
    if (destination_id == ALL_SLAVES)
      return true;
    // slave responses also to the sub handle
    if (destination_id >= 0 && destination_id == owner_id_ + 1)
      return true;
    return false;
  }

protected:
  void process(const byte *bytes, const byte length) override
  {
    auto msg = (const UartMessage *)bytes;
    if (msg->getSourceId() == owner_id_)
    {
      LOG_MESSAGED("ignored (mine)", msg, length);
    }
    else if (!for_me(msg->getDstId())) //(owner_id_ != 0xFF) && (msg->getDstId() != owner_id_) && (msg->getDstId() != ALL_SLAVES))
    {
      LOG_MESSAGED("ignored (not for me)", msg, length);
    }
    else
    {
      if (msg->getMsgType() == MsgType::MSG_LOG || msg->getMsgType() == MsgType::MSG_DUMP_LOG_REQUEST)
        LOG_MESSAGED("do", msg, length);
      else
        LOG_MESSAGED("do", msg, length);
      bool accepted = listener_ ? listener_(msg) : false;
      if (!accepted)
        LOG_MESSAGEW("not accepted!?", msg, length);
    }
  }

public:
  InteropRS485(uint8_t owner_id, Gate &gate, Buffer &buffer) : Channel(gate, buffer), owner_id_(owner_id) {}

  void setOwnerId(int id) { owner_id_ = id; }

  void send_raw(const UartMessage *msg, int bytes)
  {
    if (msg->getMsgType() == MsgType::MSG_DUMP_LOG_REQUEST)
      LOG_MESSAGED("send", msg, bytes);
    else
      LOG_MESSAGEI("send", msg, bytes);
    _send((byte *)msg, bytes);
  }

  template <class M>
  void send(const M &msg)
  {
    send_raw(&msg, sizeof(M));
  }

  void set_listener(InteropRS485ReceiverFuncPtr listener)
  {
    listener_ = listener;
  }
};

#ifdef ESP8266
namespace oclock
{

  /**
   * Note: initially, lambdas seems to very naturally to use postponing tasks.
   * Especially if you come from a Java background.
   * Put a task in a queue and execute it when it is done. This would work
   * in Java since the lambda itself is also garbage collected.
   * This, however, is not true in C++, here a lambda is undefined when it goes out of scope,
   * or its captured variables. To avoid its pitfalls I use the ExecuteRequest instead.
   *
   **/
  class ChannelRequest
  {
  private:
    void send_raw(const UartMessage *msg, const byte length);
    const std::string alias_;

  public:
    ChannelRequest(const std::string &alias) : alias_(alias) {}
    template <typename M>
    void send(const M &m)
    {
      send_raw(&m, (byte)sizeof(M));
    }
    virtual ~ChannelRequest() {}
    const std::string &get_alias() const { return alias_; }
  };

  class ExecuteRequest : public ChannelRequest
  {
  protected:
    ExecuteRequest(const std::string &alias) : ChannelRequest(alias) {}

  public:
    // 'execute' is called when the slaves are initialized
    virtual void execute() = 0;
  };

  class BroadcastRequest : public ChannelRequest
  {
  protected:
    BroadcastRequest(const std::string &alias) : ChannelRequest(alias) {}

  public:
    // 'execute' is called when the slaves are initialized
    virtual void execute() = 0;

    // 'finalize' is called only if broadcast is completed
    virtual void finalize(){
        // default is nothing
    };
  };

  // queue and its variants are implemented in master.cpp

  void queue(ExecuteRequest *request);
  void queue(BroadcastRequest *request);

  template <class M>
  void queue_message(const M &msg)
  {
    class SendRequest : public ExecuteRequest
    {
    public:
      const M msg;
      SendRequest(const M &msg) : ExecuteRequest("SendRequest"), msg(msg){};

      virtual void execute() override { send(msg); }
    };
    oclock::queue(new SendRequest(msg));
  }
} // namespace oclock
#endif
