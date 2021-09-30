#include "oclock.h"
#ifdef SLAVE_MODE

#include "pins.h"
#include "uart.h"



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