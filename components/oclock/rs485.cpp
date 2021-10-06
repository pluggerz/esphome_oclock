#include "rs485.h"
#include "oclock.h"
#include "Arduino.h"

// Helper class to calucalte CRC8

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

#define WRITE Serial.write

class RS485Protocol
{
public:
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
    RS485Protocol(const byte bufferSize) : data_(NULL),
                                           bufferSize_(bufferSize)
    {
    }

    // destructor - frees memory used
    ~RS485Protocol()
    {
        stop();
    }

    // reset to no incoming data (eg. after a timeout)
    void reset()
    {
        haveSTX_ = false;
        available_ = false;
        inputPos_ = 0;
        startTime_ = 0;
    }

    // allocate memory for buf_
    void begin()
    {
        data_ = (byte *)malloc(bufferSize_);
        reset();
        errorCount_ = 0;
    }

    // free memory in buf_
    void stop()
    {
        reset();
        free(data_);
        data_ = NULL;
    }

    // handle incoming data, return true if packet ready
    bool update();

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

}; // end of class RS485Protocol

// send a byte complemented, repeated
// only values sent would be (in hex):
//   0F, 1E, 2D, 3C, 4B, 5A, 69, 78, 87, 96, A5, B4, C3, D2, E1, F0
void RS485Protocol::sendComplemented(const byte what)
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
void RS485Protocol::sendMsg(const byte *data, const byte length)
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

bool RS485Protocol::update()
{
    // no data? can't go ahead (eg. begin() not called)
    if (data_ == NULL)
    {
        ESP_LOGD(TAG, "RS485: data_ == NULL !?");
        return false;
    }
    auto available = Serial.available();
    if (available == 0)
        return false;

    ESP_LOGD(TAG, "RS485: Serial.available() = %d", available);
    while (Serial.available() > 0)
    {
        byte inByte = Serial.read();
        switch (inByte)
        {

        case STX: // start of text
            ESP_LOGD(TAG, "RS485: received STX");
            haveSTX_ = true;
            haveETX_ = false;
            inputPos_ = 0;
            firstNibble_ = true;
            startTime_ = millis();
            break;

        case ETX: // end of text (now expect the CRC check)
            ESP_LOGD(TAG, "RS485: received ETX");
            haveETX_ = true;
            break;

        default:
            // wait until packet officially starts
            if (!haveSTX_)
            {
                ESP_LOGD(TAG, "RS485: ignoring %d (errorCount_=%d)", (int)inByte, errorCount_);
                break;
            }
            ESP_LOGD(TAG, "RS485: received %d (errorCount=%d)", (int)inByte, errorCount_);

            // check byte is in valid form (4 bits followed by 4 bits complemented)
            if ((inByte >> 4) != ((inByte & 0x0F) ^ 0x0F))
            {
                reset();
                errorCount_++;
                ESP_LOGD(TAG, "RS485: invalid nibble !? (errorCount=%d)", errorCount_);
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
                    ESP_LOGD(TAG, "RS485: bad crc !? (errorCount=%d)", errorCount_);
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
                ESP_LOGD(TAG, "RS485: overflow, start again !? (errorCount=%d)", errorCount_);
            }

            break;

        } // end of switch
    }     // end of while incoming data

    return false; // not ready yet
} // end of RS485Protocol::update

RS485::RS485(int de_pin, int re_pin) : de_pin_(de_pin),
                                       re_pin_(re_pin),
                                       protocol_(new RS485Protocol(64))
{
}

#ifdef ESP8266
bool org = true;
#else
bool org = false;
#endif

void RS485::startReceiving()
{
    ESP_LOGI(TAG, "RS485::startReceiving -> receiving = true");

    // make sure we are done with sending
    Serial.flush();

    digitalWrite(de_pin_, org ? LOW : HIGH);
    digitalWrite(re_pin_, LOW);

    // ignore input
    protocol_->reset();
    receiving = true;
}

void RS485::startTransmitting()
{
    ESP_LOGI(TAG, "RS485::startTransmitting -> receiving = false");

    digitalWrite(de_pin_, org ? HIGH : LOW);
    digitalWrite(re_pin_, LOW);

    protocol_->reset();

    receiving = false;
}

void RS485::_send(const byte *bytes, const byte length)
{
    if (receiving)
        startTransmitting();

    protocol_->sendMsg(bytes, length);
}

void RS485::setup()
{
    pinMode(de_pin_, OUTPUT);
    pinMode(re_pin_, OUTPUT);

    Serial.begin(speed);
    protocol_->begin();

    // initially we always listen
    startReceiving();

    ESP_LOGI(TAG, "UART::setup %d baud (de_pin_=%d, re_pin_=%d)", speed, de_pin_, re_pin_);
}

void RS485::dump_config()
{
    ESP_LOGCONFIG(TAG, "   UART:");
    ESP_LOGCONFIG(TAG, "      speed: %d baud", speed);
    ESP_LOGCONFIG(TAG, "      de_pin: %d", de_pin_);
    ESP_LOGCONFIG(TAG, "      re_pin: %d", re_pin_);
    ESP_LOGCONFIG(TAG, "      receiving: %s", receiving ? "true" : "false");
    ESP_LOGCONFIG(TAG, "      protocol.errorCount_: %d", protocol_->errorCount_);
}

void RS485::loop()
{
    if (!receiving)
    {
        ESP_LOGI(TAG, "Not receiving !?");
        return;
    }
    if (!protocol_->update())
        return;

    process(protocol_->getData(), protocol_->getLength());
}