#include "rs485.h"

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