#include "log.h"

#include <stdarg.h>
#include <stdio.h>

template <uint16_t SIZE>
class RingBuffer
{
    volatile uint16_t writeIndex, readIndex, bufferLength;
    volatile bool overflow_{false};
    char buffer[SIZE];

public:
    void reset()
    {
        writeIndex = readIndex = bufferLength = 0;
        overflow_ = false;
    }

    RingBuffer() { reset(); }

    HOT uint16_t size() const
    {
        return bufferLength;
    }

    HOT bool isEmpty() const
    {
        return bufferLength == 0;
    }

    HOT bool overflow() const { return overflow_; }

    HOT char pop()
    {
        // Check if buffer is empty
        if (bufferLength == 0)
        {
            // empty
            return 0;
        }

        bufferLength--; //	Decrease buffer size after reading
        char ret = buffer[readIndex];
        readIndex++; //	Increase readIndex position to prepare for next read
        overflow_ = false;

        // If at last index in buffer, set readIndex back to 0
        if (readIndex == SIZE)
        {
            readIndex = 0;
        }
        return ret;
    }

    HOT void pushInt(int val)
    {
        const int orgVal = val;
        int multiplier = 1;
        while (val > 10)
        {
            val /= 10;
            multiplier *= 10;
        }
        push((char)('0' + val));

        if (orgVal >= 10)
        {
            pushInt(orgVal - multiplier * val);
            return;
        }
    }

    HOT void push(char ch)
    {
        if (bufferLength == SIZE)
        {
            // Oops, read one line and go to the next
            pop();
            overflow_ = true;
        }

        buffer[writeIndex] = ch;

        bufferLength++; //	Increase buffer size after writing
        writeIndex++;   //	Increase writeIndex position to prepare for next write

        // If at last index in buffer, set writeIndex back to 0
        if (writeIndex == SIZE)
        {
            writeIndex = 0;
        }
    }

    HOT void push(const __FlashStringHelper *str)
    {
        PGM_P it = reinterpret_cast<PGM_P>(str);
        char ch = '@';
        while ((ch = pgm_read_byte(it++)))
        {
            push(ch);
        }
    }
};

class SlaveLogger : public LogExtractor
{
    RingBuffer<160> buffer;
#define LOG_TMP_BUFFER_SIZE 40
    char output_buffer[LOG_TMP_BUFFER_SIZE];
    bool enabled{true};

public:
    bool lock() override
    {
        enabled = false;
        return buffer.overflow();
    }

    void unlock() override
    {
        enabled = true;
    }

    HOT uint16_t size() const
    {
        return buffer.size();
    }

    HOT char pop()
    {
        return buffer.pop();
    }

    HOT void esp_log_vprintf(int level, const __FlashStringHelper *tag, int line, const __FlashStringHelper *format, va_list arg)
    {
        if (!enabled)
            return;

        // lets log
        int written = vsnprintf_P(output_buffer, LOG_TMP_BUFFER_SIZE, (PGM_P)format, arg);

        buffer.push(tag);
        buffer.push(':');
        buffer.pushInt(line);
        buffer.push('>');
        if (written < 0)
        {
            // error, just print the unformatted one
            buffer.push(format);
        }
        else
        {
            for (int idx = 0; idx < written; ++idx)
                buffer.push(output_buffer[idx]);
        }
        buffer.push('\n');
    }
};

const char *extractFileName(const __FlashStringHelper *const _path)
{
    PGM_P path = reinterpret_cast<PGM_P>(_path);

    const auto *startPosition = path;
    for (const auto *currentCharacter = path; pgm_read_byte(currentCharacter) != 0; ++currentCharacter)
    {
        char ch = pgm_read_byte(currentCharacter);
        if (ch == '\\' || ch == '/')
        {
            startPosition = currentCharacter;
        }
    }

    if (startPosition != path)
    {
        ++startPosition;
    }

    return (const char *)startPosition;
}

SlaveLogger slaveLogger;

LogExtractor *logExtractor = &slaveLogger;

HOT void esp_log_printf_(int level, const void *tag, int line, const __FlashStringHelper *format, ...)
{
    va_list arg;
    va_start(arg, format);
    slaveLogger.esp_log_vprintf(level, (const __FlashStringHelper *)tag, line, format, arg);
    va_end(arg);
}