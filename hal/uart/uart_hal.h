#ifndef UART_HAL_H
#define UART_HAL_H

#include <Arduino.h>

class UARTHAL
{
public:

    static void begin(
        HardwareSerial& serial,
        uint32_t baudrate,
        int rxPin,
        int txPin
    );

    static void write(
        HardwareSerial& serial,
        const String& data
    );

    static bool available(
        HardwareSerial& serial
    );

    static String readLine(
        HardwareSerial& serial,
        uint32_t timeout
    );
};

#endif