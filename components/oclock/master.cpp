#include "oclock.h"

#ifdef MASTER_MODE

#include "master.h"
#include "interop.h"
#include "animation.h"
#include "pins.h"
#include <deque>
#include "async.h"
#include "time_tracker.h"

using namespace oclock;

void update_from_components();

AnimationController animationController;

oclock::Master oclock::master;
bool scanForError = false;
int slaveIdCounter = -2;

std::string current_broadcast_request = "None";

byte receiverBufferBytes[RECEIVER_BUFFER_SIZE];
auto receiverBuffer = Buffer(receiverBufferBytes, RECEIVER_BUFFER_SIZE);
RS485Gate<RS485_DE_PIN, RS485_RE_PIN> gate;
InteropRS485 uart(0xFF, gate, receiverBuffer);

class MasterLifecycle
{
public:
    typedef std::function<void(Millis)> LoopFunc;
    static LoopFunc loopFunc_;

    // all steps before the master is ready
    static void change_to_init();
    static void change_to_accepting();

    // in serving mode it will accept requests from the queue
    static void change_to_serving();
    // in broadcasting mode we send a request to the slave(s) and wait until a response
    static void change_to_broadcasting(oclock::BroadcastRequest *request);
};

MasterLifecycle::LoopFunc MasterLifecycle::loopFunc_{};

void oclock::ChannelRequest::send_raw(const UartMessage *msg, const byte length)
{
    ESP_LOGI(TAG, "send_raw -> %s (%d bytes)", alias_.c_str(), length);
    uart.send_raw(msg, length);
}

void oclock::Master::setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(SYNC_IN_PIN, INPUT);
    pinMode(SYNC_OUT_PIN, OUTPUT);

    // what to do with the following pins ?
    // ?? const int MASTER_GPI0_PIN  = 0; // GPI00
    pinMode(MASTER_GPI0_PIN, INPUT);
    pinMode(ESP_TXD_PIN, OUTPUT);
    pinMode(ESP_RXD_PIN, INPUT);
    pinMode(I2C_SDA_PIN, INPUT);
    pinMode(I2C_SCL_PIN, INPUT);

    pinMode(GPIO_14, INPUT);
    pinMode(USB_POWER_PIN, INPUT);

    uart.setup();
    uart.setOwnerId(0xFF);

    // we ignore errors
    scanForError = false;
    // reset slaveIdCounter (every clock takes 2 ids so that is why it is -2)
    slaveIdCounter = -2;

    ESP_LOGI(TAG, "setup -> scanForError=false, slaveIdCounter=-2");
    ESP_LOGI(TAG, "setup -> Sync::read() = %s", ONOFF(Sync::read()));

    while (true)
    {
        delay(500);
        ESP_LOGI(TAG, "change_to_init -> PAUSE");
    }

    MasterLifecycle::change_to_init();
}

void oclock::Master::loop()
{
    if (MasterLifecycle::loopFunc_)
    {
        MasterLifecycle::loopFunc_(::millis());
    }
}

void MasterLifecycle::change_to_init()
{
    ESP_LOGI(TAG, "change_to_init");

    // force a reset, set line to high ( error mode )
    // so everyone will do the same
    Sync::write(HIGH);
    ::delay(4000);

    // force reset, give time to act
    ::delay(500);

    // we ignore errors
    scanForError = false;
    // reset slaveIdCounter (every clock takes 2 ids so that is why it is -2)
    slaveIdCounter = -2;

    ESP_LOGI(TAG, "change_to_init -> scanForError=false, slaveIdCounter=-2");
    ESP_LOGI(TAG, "change_to_init -> Sync::read() = %s", Sync::read() ? "HIGH" : "LOW");

    // all slaves will stop their work and put Sync to low
    uart.send(UartMessage(-1, MsgType::MSG_ID_RESET));
    while (Sync::read() == HIGH)
    {
        // wait while slave is high
        ::delay(200);
        ESP_LOGI(TAG, "Waiting until slaves put sync LOW...");
    }
    ESP_LOGI(TAG, "Sync is LOW...");

    // make sure we are high
    Sync::write(HIGH);

    // make sure there is enough time for everyone to act
    Sync::sleep(100);

    uart.send(UartAcceptMessage(-1, 0));
    uart.start_receiving();

    MasterLifecycle::change_to_accepting();
}

int lastLogSlaveId = -1;
#define LAST_LOG_SIZE 512
char lastLog[LAST_LOG_SIZE];
int lastLogIdx = 0;

bool processLog(UartLogMessage *msg)
{
    if (lastLogSlaveId != msg->getSourceId())
    {
        if (lastLogIdx > 0)
        {
            ESP_LOGE(TAG, "[S%d lastLogIdx > 0?:] %s", lastLogSlaveId >> 1, lastLog);
            lastLogIdx = 0;
        }
    }
    if (msg->overflow)
    {
        lastLog[lastLogIdx++] = '<';
        lastLog[lastLogIdx++] = '.';
        lastLog[lastLogIdx++] = '.';
        lastLog[lastLogIdx++] = '.';
        lastLog[lastLogIdx++] = '>';
    }
    lastLogSlaveId = msg->getSourceId();
    for (int bufferIdx = 0; bufferIdx < msg->length(); bufferIdx++)
    {
        char ch = msg->buffer[bufferIdx];
        if (ch == 0)
            break;
        lastLog[lastLogIdx++] = ch;
        lastLog[lastLogIdx] = 0;
        if (ch == '\n')
        {
            lastLog[lastLogIdx - 1] = 0;
            ESP_LOGW(TAG, "[S%d:] %s", msg->getSourceId() >> 1, lastLog);
            lastLogIdx = 0;
        }
        else if (lastLogIdx + 1 == LAST_LOG_SIZE)
        {
            // too much text
            ESP_LOGE(TAG, "[S%d too much]: %s", msg->getSourceId() >> 1, lastLog);
            lastLogIdx = 0;
        }
    }

    if (msg->part + 1 == msg->total_parts && lastLogIdx != 0)
    {
        ESP_LOGE(TAG, "[S%d missing\\n!?]: %s", lastLogSlaveId >> 1, lastLog);
        lastLogIdx = 0;
    }
    return true;
}
/***
 *
 * While accepting we expect only to receive MSG_ID_ACCEPT
 */
void MasterLifecycle::change_to_accepting()
{
    ESP_LOGI(TAG, "change_to_accepting");
    // uart
    auto listener = [](const UartMessage *msg)
    {
        if (msg->getMessageType() == MsgType::MSG_LOG)
        {
            return processLog((UartLogMessage *)msg);
        }
        bool sync = Sync::read();
        if (msg->getMessageType() != MsgType::MSG_ID_ACCEPT)
            return false;

        if (!sync)
        {
            // ignore for now
            ESP_LOGI(TAG, "Still waiting for some slaves: sync=LOW");
            return true;
        }

        slaveIdCounter = ((UartAcceptMessage *)msg)->getAssignedId();
        ESP_LOGI(TAG, "Done waiting for all slaves: sync=HIGH and slaveIdCounter=%d", slaveIdCounter);

        // send config
        for (int idx = 0; idx < MAX_SLAVES; ++idx)
        {
            const auto &s = oclock::master.slave(idx);
            if (s.animator_id == -1)
            {
                ESP_LOGW(TAG, "Slave not mapped: S%d", idx);
                continue;
            }
            animationController.remap(s.animator_id, idx);
            auto request = UartSlaveConfigRequest(idx << 1,
                                                  s.handles[0].magnet_offset, s.handles[1].magnet_offset,
                                                  s.handles[0].initial_ticks, s.handles[1].initial_ticks);
            uart.send(request);
        }

        // send DONE
        Sync::write(LOW);
        delay(100);
        uart.send(UartDoneMessage(-1, slaveIdCounter, master.get_baud_rate()));
        while (Sync::read() == HIGH)
        {
            // wait while slave is high
            ::delay(200);
            ESP_LOGI(TAG, "change_to_accepting: Wating while sync is HIGH...");
        }

        delay(500);
        uart.upgrade_baud_rate(master.get_baud_rate());
        delay(100);

        // accept errors
        scanForError = true;
        ESP_LOGI(TAG, "change_to_accepting: Sync::read()=%s", Sync::read() ? "HIGH" : "LOW");

        update_from_components();
        change_to_serving();

        return true;
    };
    uart.set_listener(listener);

    // our loop func
    loopFunc_ = [](Millis t)
    {
        uart.loop();
        ::yield();
    };
}

void MasterLifecycle::change_to_broadcasting(oclock::BroadcastRequest *request)
{
    current_broadcast_request = request->get_alias();
#define FINAL_REQUEST()                       \
    {                                         \
        if (request)                          \
        {                                     \
            request->finalize();              \
            delete request;                   \
        }                                     \
        current_broadcast_request = "None";   \
        MasterLifecycle::change_to_serving(); \
    }

    uart.start_receiving();
    ESP_LOGI(TAG, "change_to_broadcasting");

    // note: we copy by value since the stack will invalid at the point we need it!
    auto listener = [request](const UartMessage *msg)
    {
        switch (msg->getMessageType())
        {
        case MsgType::MSG_LOG:
            return processLog((UartLogMessage *)msg);

        case MsgType::MSG_DUMP_LOG_REQUEST:
            if (msg->getDstId() == 0xFF)
            {
                ESP_LOGI(TAG, "Done dumping logs request! > %d", msg->getDstId());
                FINAL_REQUEST()
            }
            else
            {
                ESP_LOGI(TAG, "Waiting before dumping logs request! > %d", msg->getDstId());
            }
            return true;

        case MsgType::MSG_WAIT_FOR_ANIMATION:
            if (msg->getDstId() == 0xFF)
            {
                ESP_LOGI(TAG, "Done MSG_WAIT_FOR_ANIMATION request! > %d ", msg->getDstId());
                FINAL_REQUEST()
            }
            else
            {
                ESP_LOGI(TAG, "Waiting before MSG_WAIT_FOR_ANIMATION request! > %d ", msg->getDstId());
            }
            return true;

        case MsgType::MSG_POS_REQUEST:
        {
            auto pos_msg = reinterpret_cast<const UartPosRequest *>(msg);
            ESP_LOGI(TAG, "Store pos request! %d %d (%d, %d)", msg->getSourceId(), slaveIdCounter, pos_msg->pos0, pos_msg->pos1);
            animationController.set_handles(msg->getSourceId(), pos_msg->pos0, pos_msg->pos1);
            if (msg->getDstId() == 0xFF)
            {
                ESP_LOGI(TAG, "Done retrieving pos request! > %d ", msg->getDstId());
                FINAL_REQUEST()
            }
            else
            {
                ESP_LOGI(TAG, "Waiting before retrieving pos request! > %d ", msg->getDstId());
            }
        }
            return true;

        default:
            return false;
        }
#undef FINAL_REQUEST
    };
    uart.set_listener(listener);

    // our loop func
    loopFunc_ = [](Millis now)
    {
        AsyncRegister::loop(now);
        uart.loop();
        ::yield();
    };
}

std::deque<oclock::ExecuteRequest *> open_requests;

void dump_open_requests()
{
    ESP_LOGI(TAG, "queue.size: %d (current broad_cast: %s)", open_requests.size(), current_broadcast_request.c_str());
    for (auto it = open_requests.begin(); it != open_requests.end(); ++it)
    {
        ESP_LOGI(TAG, " -- %s", (*it)->get_alias().c_str());
    }
}

void MasterLifecycle::change_to_serving()
{
    ESP_LOGI(TAG, "change_to_serving");
    uart.start_receiving();
    auto listener = [](const UartMessage *msg)
    {
        switch (msg->getMessageType())
        {
        case MsgType::MSG_LOG:
        case MsgType::MSG_POS_REQUEST:
            LOG_MESSAGEW("should not happen!?", msg, -1);
            return false;

        default:
            return false;
        }
    };
    uart.set_listener(listener);

    loopFunc_ = [](Millis now)
    {
        if (!open_requests.empty())
        {
            oclock::ExecuteRequest *request = open_requests.front();
            ESP_LOGI(TAG, "executing/popped: %s", request->get_alias().c_str());
            open_requests.pop_front();
            dump_open_requests();
            // execute
            request->execute();
            delete request;
            // jump from the loop
            return;
        }
        AsyncRegister::loop(now);
        // uart.loop();
        ::yield();
    };
}

void oclock::Master::dump_config()
{
    auto tag = TAG;
    ESP_LOGI(tag, "MASTER:");
    ESP_LOGI(tag, "  queue.size: %d (current broad_cast: %s)", open_requests.size(), current_broadcast_request.c_str());
    for (auto it = open_requests.begin(); it != open_requests.end(); ++it)
    {
        ESP_LOGI(TAG, "               -- %s", (*it)->get_alias().c_str());
    }
    ESP_LOGI(tag, "  brightness: %d", brightness_);
    ESP_LOGI(tag, "  time trackers:");
    oclock::time_tracker::realTimeTracker.dump_config(tag);
    oclock::time_tracker::testTimeTracker.dump_config(tag);
    oclock::time_tracker::internalClockTimeTracker.dump_config(tag);

    uart.dump_config(tag);

    animationController.dump_config(tag);
}

void oclock::queue(ExecuteRequest *request)
{
    open_requests.push_back(request);
    dump_open_requests();
}

void oclock::queue(oclock::BroadcastRequest *request)
{
    class CallbackRequest final : public oclock::ExecuteRequest
    {
        oclock::BroadcastRequest *org_request_;

    public:
        CallbackRequest(oclock::BroadcastRequest *org_request) : ExecuteRequest(std::string("CallbackRequest+") + org_request->get_alias()), org_request_(org_request) {}

        virtual ~CallbackRequest()
        {
            if (org_request_)
                delete org_request_;
        }

        void execute()
        {
            org_request_->execute();
            MasterLifecycle::change_to_broadcasting(org_request_);
            org_request_ = nullptr;
        }
    };

    queue(new CallbackRequest(request));
}

void oclock::Master::reset()
{
    AsyncRegister::byName("time_tracker", nullptr);
    while (!open_requests.empty())
    {
        delete open_requests.back();
        open_requests.pop_back();
    }
    MasterLifecycle::change_to_init();
}

#endif