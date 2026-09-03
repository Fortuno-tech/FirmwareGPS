#include "sim7600_driver.h"

#include "uart_hal.h"
#include "pin_config.h"
#include "hardware_config.h"

SIM7600Driver::SIM7600Driver()
    : _serial(nullptr),
      _ready(false)
{
}

bool SIM7600Driver::begin()
{
    Serial.println();
    Serial.println(
        "================================="
    );

    Serial.println(
        "       SIM7600 DRIVER"
    );

    Serial.println(
        "================================="
    );

    _serial = new HardwareSerial(2);

    UARTHAL::begin(
        *_serial,
        SIM7600_BAUDRATE,
        SIM7600_RX_PIN,
        SIM7600_TX_PIN
    );

    delay(1000);

    if (!testAT())
    {
        Serial.println(
            "SIM7600 : PAS DE REPONSE"
        );

        _ready = false;

        return false;
    }

    _ready = true;

    Serial.println(
        "SIM7600 : OK"
    );

    return true;
}

bool SIM7600Driver::testAT()
{
    String response =
        sendCommand(
            "AT",
            3000
        );

    Serial.print(
        "AT -> "
    );

    Serial.println(
        response
    );

    return (
        response.indexOf("OK")
        >= 0
    );
}

String SIM7600Driver::sendCommand(
    const String& command,
    uint32_t timeout
)
{
    if (_serial == nullptr)
    {
        return "";
    }

    while (_serial->available())
    {
        _serial->read();
    }

    _serial->print(command);
    _serial->print("\r\n");

    String response;

    uint32_t start = millis();

    while (
        millis() - start < timeout
    )
    {
        while (_serial->available())
        {
            char c =
                _serial->read();

            response += c;
        }

        if (
            response.indexOf(
                "\r\nOK"
            ) >= 0
        )
        {
            break;
        }

        if (
            response.indexOf(
                "\r\nERROR"
            ) >= 0
        )
        {
            break;
        }

        delay(1);
    }

    return response;
}

bool SIM7600Driver::isReady() const
{
    return _ready;
}