#include "oclock.h"

#include "uart.h"

// extern uart::UARTComponent *uart_uartcomponent;

#define WRITE Serial.write
// #define WRITE uart_uartcomponent->write

#ifdef MASTER_MODE

std::ostream &operator<<(std::ostream &os, MsgType ethertype)
{
    switch (ethertype)
    {
    case MsgType::MSG_ID_RESET:
        return os << "MSG_ID_RESET";
    case MsgType::MSG_ID_START:
        return os << "MSG_ID_START";
    case MsgType::MSG_ID_ACCEPT:
        return os << "MSG_ID_ACCEPT";
    case MsgType::MSG_ID_DONE:
        return os << "MSG_ID_DONE";
    case MsgType::MSG_CALIBRATE_START:
        return os << "MSG_CALIBRATE_START";
    case MsgType::MSG_CALIBRATE_END:
        return os << "MSG_CALIBRATE_END";
    case MsgType::MSG_POS_REQUEST:
        return os << "MSG_POS_REQUEST";
    case MsgType::MSG_BEGIN_KEYS:
        return os << "MSG_BEGIN_KEYS";
    case MsgType::MSG_SEND_KEYS:
        return os << "MSG_SEND_KEYS";
    case MsgType::MSG_END_KEYS:
        return os << "MSG_END_KEYS";
    case MsgType::MSG_COLOR:
        return os << "MSG_COLOR";
    case MsgType::MSG_COLOR_ANIMATION:
        return os << "MSG_COLOR_ANIMATION";
        // omit default case to trigger compiler warning for missing cases
    };
    return os << "MsgType@" << static_cast<std::uint16_t>(ethertype);
}

std::ostream &operator<<(std::ostream &out, const Cmd &c)
{
    out << (c.mode & CmdEnum::ABSOLUTE ? " abs " : " rel ")
        << (c.mode & CmdEnum::CLOCK_WISE ? " --> " : " <-- ") << std::dec << (int)c.steps << " "
        << "speed[" << (int)c.speed << "] "
        << "raw[0x" << std::hex << c.asRaw() << "] ";
    return out;
}

#endif

class CRC8
{
public:
    static byte calc(const byte *addr, byte len)
    {
        byte crc = 0;
        while (len--)
        {
            byte inbyte = *addr++;
            for (byte i = 8; i; i--)
            {
                byte mix = (crc ^ inbyte) & 0x01;
                crc >>= 1;
                if (mix)
                    crc ^= 0x8C;
                inbyte >>= 1;
            } // end of for
        }     // end of while
        return crc;
    }
};

// based on: http://www.gammon.com.au/forum/?id=11428

class RS485
{
    enum
    {
        STX = '\2', // start of text
        ETX = '\3'  // end of text
    };              // end of enum

    // where we save incoming stuff
    byte *data_;

    // how much data is in the buffer
    const int bufferSize_;

    // this is true once we have valid data in buf
    bool available_;

    // an STX (start of text) signals a packet start
    bool haveSTX_;

    // count of errors
    unsigned long errorCount_;

    // variables below are set when we get an STX
    bool haveETX_;
    byte inputPos_;
    byte currentByte_;
    bool firstNibble_;
    unsigned long startTime_;

    // helper private functions
    byte crc8(const byte *addr, byte len) { return CRC8::calc(addr, len); }
    void sendComplemented(const byte what);

public:
    // constructor
    RS485(const byte bufferSize) : data_(NULL),
                                   bufferSize_(bufferSize)
    {
    }

    // destructor - frees memory used
    ~RS485()
    {
        stop();
    }

    // allocate memory for buf_
    void begin();

    // free memory in buf_
    void stop();

    // handle incoming data, return true if packet ready
    bool update();

    // reset to no incoming data (eg. after a timeout)
    void reset();

    // send data
    void sendMsg(const byte *data, const byte length);

    // returns true if packet available
    bool available() const
    {
        return available_;
    };

    byte *getData() const
    {
        return data_;
    }
    byte getLength() const
    {
        return inputPos_;
    }

    // return how many errors we have had
    unsigned long getErrorCount() const
    {
        return errorCount_;
    }

    // return when last packet started
    unsigned long getPacketStartTime() const
    {
        return startTime_;
    }

    // return true if a packet has started to be received
    bool isPacketStarted() const
    {
        return haveSTX_;
    }

}; // end of class RS485

// allocate the requested buffer size
void RS485::begin()
{
    data_ = (byte *)malloc(bufferSize_);
    reset();
    errorCount_ = 0;
} // end of RS485::begin

// get rid of the buffer
void RS485::stop()
{
    reset();
    free(data_);
    data_ = NULL;
} // end of RS485::stop

// called after an error to return to "not in a packet"
void RS485::reset()
{
    haveSTX_ = false;
    available_ = false;
    inputPos_ = 0;
    startTime_ = 0;
} // end of RS485::reset

// send a byte complemented, repeated
// only values sent would be (in hex):
//   0F, 1E, 2D, 3C, 4B, 5A, 69, 78, 87, 96, A5, B4, C3, D2, E1, F0
void RS485::sendComplemented(const byte what)
{
    byte c;

    // first nibble
    c = what >> 4;
    WRITE((c << 4) | (c ^ 0x0F));

    // second nibble
    c = what & 0x0F;
    WRITE((c << 4) | (c ^ 0x0F));
} // end of RS485::sendComplemented

// send a message of "length" bytes (max 255) to other end
// put STX at start, ETX at end, and add CRC
void RS485::sendMsg(const byte *data, const byte length)
{
    WRITE(STX); // STX
    for (byte i = 0; i < length; i++)
        sendComplemented(data[i]);
    WRITE(ETX); // ETX
    sendComplemented(crc8(data, length));
} // end of RS485::sendMsg

// called periodically from main loop to process data and
// assemble the finished packet in 'data_'

// returns true if packet received.

// You could implement a timeout by seeing if isPacketStarted() returns
// true, and if too much time has passed since getPacketStartTime() time.

bool RS485::update()
{
    // no data? can't go ahead (eg. begin() not called)
    if (data_ == NULL)  {
        ESP_LOGCONFIG(TAG, "RS485: data_ == NULL !?");
        return false;
    }
    auto available=Serial.available();
    if (available == 0)
        return false;
    
    ESP_LOGCONFIG(TAG, "RS485: Serial.available() = %d", available);
    while (Serial.available() > 0)
    {
        byte inByte = Serial.read();
        switch (inByte)
        {

        case STX: // start of text
            ESP_LOGCONFIG(TAG, "RS485: received STX");
            haveSTX_ = true;
            haveETX_ = false;
            inputPos_ = 0;
            firstNibble_ = true;
            startTime_ = millis();
            break;

        case ETX: // end of text (now expect the CRC check)
            ESP_LOGCONFIG(TAG, "RS485: received ETX");
            haveETX_ = true;
            break;

        default:
            // wait until packet officially starts
            if (!haveSTX_)
            {
                ESP_LOGCONFIG(TAG, "RS485: ignoring %d (errorCount_=%d)", (int)inByte, errorCount_);
                break;
            }
            ESP_LOGCONFIG(TAG, "RS485: received %d (errorCount=%d)", (int)inByte, errorCount_);

            // check byte is in valid form (4 bits followed by 4 bits complemented)
            if ((inByte >> 4) != ((inByte & 0x0F) ^ 0x0F))
            {
                ESP_LOGCONFIG(TAG, "RS485: invalid nibble !? (errorCount=%d)", errorCount_);
                reset();
                errorCount_++;
                break; // bad character
            }          // end if bad byte

            // convert back
            inByte >>= 4;

            // high-order nibble?
            if (firstNibble_)
            {
                currentByte_ = inByte;
                firstNibble_ = false;
                break;
            } // end of first nibble

            // low-order nibble
            currentByte_ <<= 4;
            currentByte_ |= inByte;
            firstNibble_ = true;

            // if we have the ETX this must be the CRC
            if (haveETX_)
            {
                if (crc8(data_, inputPos_) != currentByte_)
                {
                    reset();
                    errorCount_++;
                    break; // bad crc
                }          // end of bad CRC

                available_ = true;
                return true; // show data ready
            }                // end if have ETX already

            // keep adding if not full
            if (inputPos_ < bufferSize_)
                data_[inputPos_++] = currentByte_;
            else
            {
                reset(); // overflow, start again
                errorCount_++;
            }

            break;

        } // end of switch
    }     // end of while incoming data

    return false; // not ready yet
} // end of RS485::update

RS485 rs485(MAX_UART_MESSAGE_SIZE);

void UART::send(const byte *bytes, const byte length)
{
    rs485.sendMsg(bytes, length);
}

void UART::setup()
{
    pinMode(de_pin_, OUTPUT);
    pinMode(re_pin_, OUTPUT);
    
    //digitalWrite(de_pin_, HIGH);
    //digitalWrite(re_pin_, HIGH);
    //delay(100);

    Serial.begin(speed);
    rs485.begin();
    
    ESP_LOGI(TAG, "UART::setup %d baud (de_pin_=%d, re_pin_=%d)", speed,de_pin_, re_pin_);
}

void UART::dump_config()
{
    ESP_LOGCONFIG(TAG, "   UART:");
    ESP_LOGCONFIG(TAG, "      speed: %d baud", speed);
    ESP_LOGCONFIG(TAG, "      de_pin: %d", de_pin_);
    ESP_LOGCONFIG(TAG, "      re_pin: %d", re_pin_);
}

void UART::loop(Micros now)
{
    if (!receiving)
    {
        ESP_LOGI(TAG, "Not receiving !?");
        return;
    }
    if (!rs485.update())
        return;

    // packet !
    UartMessageHolder holder(rs485.getData(), rs485.getLength());
    process(holder);
}

void UART::startTransmitting()
{
    ESP_LOGI(TAG, "UART::startTransmitting -> receiving = false");

    digitalWrite(de_pin_, HIGH);
    digitalWrite(re_pin_, LOW);

    rs485.reset();
    
    receiving = false;
}

void UART::startReceiving()
{
    ESP_LOGI(TAG, "UART::startReceiving -> receiving = true");

    // make sure we are done with sending
    Serial.flush();

    digitalWrite(de_pin_, LOW);
    digitalWrite(re_pin_, LOW);

    // ignore input
    rs485.reset();
    receiving = true;
}
