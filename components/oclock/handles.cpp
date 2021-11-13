#ifdef ESP8266

#include "handles.h"

ClockCharacter ZERO = ClockCharacter(
    ClockHandleRow(3, 6, 6, 9),
    ClockHandleRow(0, 6, 0, 6),
    ClockHandleRow(0, 3, 0, 9));

ClockCharacter ONE = ClockCharacter(
    ClockHandleRow(-1, -1, 6, 6),
    ClockHandleRow(-1, -1, 0, 6),
    ClockHandleRow(-1, -1, 0, 0));

ClockCharacter TWO = ClockCharacter(
    ClockHandleRow(3, 3, 9, 6),
    ClockHandleRow(3, 6, 0, 9),
    ClockHandleRow(0, 3, 9, 9));

ClockCharacter THREE = ClockCharacter(
    ClockHandleRow(3, 3, 6, 9),
    ClockHandleRow(3, 3, 0, 9),
    ClockHandleRow(3, 3, 0, 9));

ClockCharacter FOUR = ClockCharacter(
    ClockHandleRow(6, 6, 6, 6),
    ClockHandleRow(0, 3, 0, 6),
    ClockHandleRow(-1, -1, 0, 0));

ClockCharacter FIVE = ClockCharacter(
    ClockHandleRow(3, 6, 9, 9),
    ClockHandleRow(0, 3, 6, 9),
    ClockHandleRow(3, 3, 0, 9));

ClockCharacter SIX = ClockCharacter(
    ClockHandleRow(3, 6, 9, 9),
    ClockHandleRow(0, 6, 6, 9),
    ClockHandleRow(0, 3, 0, 9));

ClockCharacter SEVEN = ClockCharacter(
    ClockHandleRow(3, 3, 6, 9),
    ClockHandleRow(-1, -1, 0, 6),
    ClockHandleRow(-1, -1, 0, 0));

ClockCharacter EIGHT = ClockCharacter(
    ClockHandleRow(3, 6, 6, 9),
    ClockHandleRow(0, 3, 6, 9),
    ClockHandleRow(0, 3, 0, 9));

const ClockCharacter NINE = ClockCharacter(
    ClockHandleRow(3, 6, 6, 9),
    ClockHandleRow(0, 3, 0, 6),
    ClockHandleRow(3, 3, 0, 9));

const ClockCharacter FORCED_ZERO = ClockCharacter(
    ClockHandleRow(0, 1, 0, 1),
    ClockHandleRow(0, 1, 0, 1),
    ClockHandleRow(0, 1, 0, 1));

const ClockCharacter FORCED_ONE = ClockCharacter(
    ClockHandleRow(1, 2, 1, 2),
    ClockHandleRow(1, 2, 1, 2),
    ClockHandleRow(1, 2, 1, 2));

const ClockCharacter EMPTY = ClockCharacter(
    ClockHandleRow(-1, -1, -1, -1),
    ClockHandleRow(-1, -1, -1, -1),
    ClockHandleRow(-1, -1, -1, -1));

const ClockCharacter *NMBRS[] = {
    &ZERO, &ONE, &TWO, &THREE, &FOUR, &FIVE, &SIX, &SEVEN, &EIGHT, &NINE};

#endif