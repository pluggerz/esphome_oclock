#include "channel.h"
#include "hal.h"

// Helper class to calucalte CRC8

uint32_t initial_baud_rate{57600};

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

class Protocol
{
public:
    enum
    {
        STX = '\2', // start of text
        ETX = '\3'  // end of text
    };              // end of enum

    // where we save incoming stuff
    Gate &gate;
    Buffer &buffer;

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

    // send a byte complemented, repeated
    // only values sent would be (in hex):
    //   0F, 1E, 2D, 3C, 4B, 5A, 69, 78, 87, 96, A5, B4, C3, D2, E1, F0
    inline void sendComplemented(const byte what)
    {
        byte c;

        // first nibble
        c = what >> 4;
        Serial.write((c << 4) | (c ^ 0x0F));
        Hal::yield();

        // second nibble
        c = what & 0x0F;
        Serial.write((c << 4) | (c ^ 0x0F));
        Hal::yield();
    } // end of RS485::sendComplemented

public:
    // constructor
    Protocol(Gate &gate, Buffer &buffer) : gate(gate), buffer(buffer)
    {
    }

    // destructor - frees memory used
    ~Protocol()
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

    // reset
    void begin()
    {
        reset();
        errorCount_ = 0;
    }

    // free memory in buf_
    void stop()
    {
        reset();
    }

    // handle incoming data, return true if packet ready
    bool update();

    inline void wait_for_room()
    {
        return;

        bool waited = false;
        int av = Serial.availableForWrite();
        while (av < 20)
        {
            if (!waited)
            {
                ESP_LOGW(TAG, "Waited for available: %d", av);
            }
            waited = true;
            delay(3); // FIX CRC ERRORS ?
            av = Serial.availableForWrite();
        }
    }

    // send a message of "length" bytes (max 255) to other end
    // put STX at start, ETX at end, and add CRC
    inline void sendMsg(const byte *data, const byte length)
    {
        wait_for_room();
        Serial.write(STX); // STX
        Hal::yield();
        for (byte i = 0; i < length; i++)
        {
            wait_for_room();
            sendComplemented(data[i]);
        }
        Serial.write(ETX); // ETX
        Hal::yield();
        sendComplemented(crc8(data, length));
    } // end of RS485::sendMsg

    // returns true if packet available
    bool available() const
    {
        return available_;
    };

    byte *getData() const
    {
        return buffer.raw();
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

// called periodically from main loop to process data and
// assemble the finished packet in 'buffer_'

// returns true if packet received.

// You could implement a timeout by seeing if isPacketStarted() returns
// true, and if too much time has passed since getPacketStartTime() time.

bool Protocol::update()
{
    int available = Serial.available();
    if (available == 0)
        return false;

    ESP_LOGD(TAG, "RS485: available() = %d", available);
    while (Serial.available() > 0)
    {
        byte inByte = Serial.read();
        switch (inByte)
        {
        case STX: // start of text
            ESP_LOGD(TAG, "RS485: STX  (E=%ld)", errorCount_);
            haveSTX_ = true;
            haveETX_ = false;
            inputPos_ = 0;
            firstNibble_ = true;
            startTime_ = millis();
            break;

        case ETX: // end of text (now expect the CRC check)
            ESP_LOGD(TAG, "RS485: ETX  (E=%ld)", errorCount_);
            haveETX_ = true;
            break;

        default:
            // wait until packet officially starts
            if (!haveSTX_)
            {
                ESP_LOGD(TAG, "ignoring %d (E=%ld)", (int)inByte, errorCount_);
                break;
            }
            ESP_LOGVV(TAG, "received nibble %d (E=%ld)", (int)inByte, errorCount_);

            // check byte is in valid form (4 bits followed by 4 bits complemented)
            if ((inByte >> 4) != ((inByte & 0x0F) ^ 0x0F))
            {
                reset();
                errorCount_++;
                ESP_LOGE(TAG, "invalid nibble !? (E=%ld)", errorCount_);
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
                if (crc8(buffer.raw(), inputPos_) != currentByte_)
                {
                    reset();
                    errorCount_++;
                    ESP_LOGE(TAG, "CRC!? (E=%ld)", errorCount_);
                    break; // bad crc
                }          // end of bad CRC

                available_ = true;
                return true; // show data ready
            }                // end if have ETX already

            // keep adding if not full
            if (inputPos_ < buffer.size())
            {
                ESP_LOGVV(TAG, "received byte %d (E=%ld)", (int)currentByte_, errorCount_);
                buffer.raw()[inputPos_++] = currentByte_;
            }
            else
            {
                reset(); // overflow, start again
                errorCount_++;
                ESP_LOGE(TAG, "OVERFLOW !? (E=%ld)", errorCount_);
            }

            break;

        } // end of switch
    }     // end of while incoming data

    return false; // not ready yet
} // end of Protocol::update

Channel::Channel(Gate &gate, Buffer &buffer)
    : baud_rate(initial_baud_rate),
      gate(gate),
      protocol_(new Protocol(gate, buffer))
{
}

void Channel::skip()
{
    while (Serial.available() > 0)
    {
        Serial.read();
    }
}

void Channel::start_receiving()
{
    if (receiving)
        return;

    ESP_LOGD(TAG, "receiving=T");

    // make sure we are done with sending
    Serial.flush();

    gate.start_receiving();

    // ignore input
    protocol_->reset();
    receiving = true;
}

void Channel::start_transmitting()
{
    // delay(10);
    ESP_LOGD(TAG, "receiving=F");

    gate.start_transmitting();

    protocol_->reset();

    receiving = false;
}

void Channel::_send(const byte *bytes, const byte length)
{
    if (receiving)
    {
        // delay(200);
        start_transmitting();
        // delay(10);
    }
    protocol_->sendMsg(bytes, length);
}

void Channel::downgrade_baud_rate()
{
    upgrade_baud_rate(initial_baud_rate);
}

void Channel::upgrade_baud_rate(uint32_t value)
{
    if (baud_rate == value)
        return;

    Serial.end();
    ESP_LOGI(TAG, "UP: %ld -> %ld baud", baud_rate, value);
    baud_rate = value;

    Serial.begin(baud_rate);
    protocol_->reset();

    start_transmitting();
}

void Channel::setup()
{
    gate.setup();

    Serial.begin(baud_rate);
    protocol_->begin();

    // initially we always listen
    start_receiving();

    ESP_LOGI(TAG, "setup: %ld baud", baud_rate);
}

void Channel::dump_config(const char *tag)
{
    ESP_LOGI(tag, " channel(%ld baud):", baud_rate);
    ESP_LOGI(tag, "  gate:");
    gate.dump_config(tag);
    ESP_LOGI(tag, "  receiving: %s", receiving ? "T" : "F");
    ESP_LOGI(tag, "  # errors: %ld", protocol_->errorCount_);
}

void Channel::loop()
{
    if (!receiving)
    {
        ESP_LOGE(TAG, "Not receiving !?");
        return;
    }
    if (!protocol_->update())
        return;

    process(protocol_->getData(), protocol_->getLength());
}