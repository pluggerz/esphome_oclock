#pragma once

#include <functional>

#define UNUSED_OFFSET 13

class ClockHandles
{
private:
    int8_t toOffset(int pos) const
    {
        if (pos == -1)
        {
            return UNUSED_OFFSET;
        }
        return pos;
    }
    const int8_t shortHandle;
    const int8_t longHandle;

public:
    ClockHandles(int8_t shortHandle, int8_t longHandle) : shortHandle(shortHandle), longHandle(longHandle) {}

    int8_t shortHandleInOffset() const
    {
        return toOffset(shortHandle);
    }

    int8_t longHandleInOffset() const
    {
        return toOffset(longHandle);
    }
};

class ClockHandleRow
{
    const ClockHandles handles[2];

public:
    const ClockHandles &operator[](int idx) const
    {
        return handles[idx];
    }

    ClockHandleRow(int8_t leftShort, int8_t leftLong, int8_t rightShort, int8_t rightLong)
        : handles{ClockHandles(leftShort, leftLong), ClockHandles(rightShort, rightLong)} {}
};

class ClockCharacter
{
public:
    const ClockHandleRow rows[3];

    ClockCharacter(const ClockHandleRow &row0, const ClockHandleRow &row1, const ClockHandleRow &row2)
        : rows{row0, row1, row2} {}

    const ClockHandleRow &operator[](int idx) const
    {
        return rows[idx];
    }
};

extern ClockCharacter ZERO;
extern ClockCharacter ONE;
extern ClockCharacter TWO;
extern ClockCharacter THREE;
extern ClockCharacter FOUR;
extern ClockCharacter FIVE;
extern ClockCharacter SIX;
extern ClockCharacter SEVEN;
extern ClockCharacter EIGHT;
extern const ClockCharacter NINE;
extern const ClockCharacter FORCED_ZERO;
extern const ClockCharacter FORCED_ONE;
extern const ClockCharacter EMPTY;

extern const ClockCharacter *NMBRS[];

class ClockCharacters
{
    const ClockCharacter *chars[4];

public:
    ClockCharacters(const ClockCharacter *ch0, const ClockCharacter *ch1, const ClockCharacter *ch2, const ClockCharacter *ch3)
        : chars{ch0, ch1, ch2, ch3} {}

    const ClockCharacter &operator[](int idx) const
    {
        return *(chars[idx]);
    }
};

class ClockUtil
{
private:
    static const ClockCharacter *from_character(char ch)
    {
        if (ch >= '0' && ch <= '9')
            return fromNumber(ch - '0');
        return &EMPTY;
    }

    static const ClockCharacter *fromNumber(int nmbr)
    {
        return nmbr < 0 || nmbr > 9 ? &EMPTY : NMBRS[nmbr];
    }

    static void set(uint8_t *handles, int slaveId, int minor, int major)
    {
        if (minor == -1)
        {
            minor = 13;
        }
        if (major == -1)
        {
            major = 13;
        }
        handles[slaveId] = (uint8_t)((major << 4) | minor);
    }

    static void fillHandle(uint8_t *handles, int slaveId, const ClockCharacter &ch)
    {
        for (auto rowId = 0; rowId < 3; ++rowId)
        {
            const ClockHandleRow &row = ch[rowId];
            set(handles, slaveId + rowId * 2, row[0].shortHandleInOffset(), row[0].longHandleInOffset());
            set(handles, slaveId + rowId * 2 + 1, row[1].shortHandleInOffset(), row[1].longHandleInOffset());
        }
    }

    static void iterateClocks(int baseClockId, const ClockCharacter &ch, std::function<void(int, int, int)> func)
    {
        for (auto rowId = 0; rowId < 3; ++rowId)
        {
            const ClockHandleRow &row = ch[rowId];
            auto clockId = baseClockId + rowId * 2;
            func(clockId, row[0].shortHandleInOffset(), row[0].longHandleInOffset());
            func(clockId + 1, row[1].shortHandleInOffset(), row[1].longHandleInOffset());
        }
    }

    inline static void swap(uint8_t &a, uint8_t &b)
    {
        uint8_t c = a;
        a = b;
        b = c;
    }

    // static void mapToPhyscial(uint8_t*handles) {
    //   swap(handles[0], handles[1]);
    // }

public:
    static ClockCharacters retrieveClockCharactersfromCharacters(char ch0, char ch1, char ch2, char ch3)
    {
        return ClockCharacters(from_character(ch0), from_character(ch1), from_character(ch2), from_character(ch3));
    }

    static ClockCharacters retrieveClockCharactersfromNumbers(int n0, int n1, int n2, int n3)
    {
        return ClockCharacters(fromNumber(n0), fromNumber(n1), fromNumber(n2), fromNumber(n3));
    }

    static ClockCharacters retrieveClockCharactersfromDigitalTime(int hours, int minutes)
    {
        return retrieveClockCharactersfromNumbers(hours / 10, hours % 10, minutes / 10, minutes % 10);
    }

    static void convertToRawHandles(ClockCharacters srcChars, uint8_t *dstHandles)
    {
        fillHandle(dstHandles, 0, srcChars[0]);
        fillHandle(dstHandles, 6, srcChars[1]);
        fillHandle(dstHandles, 12, srcChars[2]);
        fillHandle(dstHandles, 18, srcChars[3]);
    }

    static void iterate_clocks(const ClockCharacters &srcChars, std::function<void(int clockId, int handle0, int handle1)> func)
    {
        iterateClocks(0, srcChars[0], func);
        iterateClocks(6, srcChars[1], func);
        iterateClocks(12, srcChars[2], func);
        iterateClocks(18, srcChars[3], func);
    }

    static void iterate_handles(const ClockCharacters &srcChars, std::function<void(int, int)> func)
    {
        std::function<void(int, int, int)> innerFunc = [&func](int clockId, int shortHandle, int longHandle)
        {
            auto handleId = clockId * 2;
            func(handleId, shortHandle);
            func(handleId + 1, longHandle);
        };
        iterate_clocks(srcChars, innerFunc);
    }
};
