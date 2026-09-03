#include "uart_hal.h"

void UARTHAL::begin(
    HardwareSerial& serial,
    uint32_t baudrate,
    int rxPin,
    int txPin
)
{
    serial.begin(
        baudrate,
        SERIAL_8N1,
        rxPin,
        txPin
    );
}

void UARTHAL::write(
    HardwareSerial& serial,
    const String& data
)
{
    serial.print(data);
}

bool UARTHAL::available(
    HardwareSerial& serial
)
{
    return serial.available() > 0;
}

String UARTHAL::readLine(
    HardwareSerial& serial,
    uint32_t timeout
)
{
    uint32_t start = millis();

    String response;

    while (
        millis() - start < timeout
    )
    {
        while (serial.available())
        {
            char c = serial.read();

            if (c == '\n')
            {
                return response;
            }

            if (c != '\r')
            {
                response += c;
            }
        }

        delay(1);
    }

    return response;
}