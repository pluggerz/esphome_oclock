#pragma once

class Hal
{
public:
    typedef void (*HighPriorityWorkFunc)();
    typedef void (*DumpFunc)(const char* tag);

private:
    static HighPriorityWorkFunc yield_func;

public:
    inline static void set_yield_func(HighPriorityWorkFunc func)
    {
        yield_func = func;
    }

    inline static void yield()
    {
        if (yield_func)
            yield_func();
    }


    static void register_dump(DumpFunc func);
};