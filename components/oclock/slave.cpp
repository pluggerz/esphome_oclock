#include "oclock.h"

#include "pins.h"
#include "uart.h"


#ifdef SLAVE_MODE

UART uart(RS485_DE_PIN, RS485_RE_PIN);

void setup()    {
    uart.setup();
}

void loop() {
    auto t=micros();
    uart.loop(t);
}

void UART::process(const UartMessageHolder &holder)
{
}

#endif