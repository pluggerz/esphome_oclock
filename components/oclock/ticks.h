#pragma once

#include "oclock.h"

class Ticks
{
public:
    static int normalize(int value)
    {
        while (value < 0)
            value += NUMBER_OF_STEPS;
        while (value >= NUMBER_OF_STEPS)
            value -= NUMBER_OF_STEPS;
        return value;
    }
};

class Distance
{
    // some distance functions, where we assume that from, to in [0..NUMBER_OF_STEPS)
public:
    inline static int clockwise(int from, int to)
    {
        if (from <= to)
        {
            return to - from;
        }
        else
        {
            return NUMBER_OF_STEPS - (from - to);
        }
    }

    inline static int antiClockwise(int from, int to)
    {
        return clockwise(to, from);
    }

    inline static int shortestDirection(int from, int to)
    {
        int dist0 = clockwise(from, to);
        int dist1 = antiClockwise(from, to);
        return dist0 < dist1 ? dist0 : -dist1;
    }

    /*
    static int shortestPath(int from, int to)
    {
        return abs(shortestDirection(from, to));
    }*/
};
