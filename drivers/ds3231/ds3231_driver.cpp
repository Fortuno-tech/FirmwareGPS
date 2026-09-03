#include "ds3231_driver.h"

#include <Wire.h>

#include "pin_config.h"
#include "rtc_hal.h"

DS3231Driver::DS3231Driver()
    : _ready(false)
{
}

bool DS3231Driver::begin()
{
    Serial.println();
    Serial.println(
        "================================="
    );

    Serial.println(
        "         DS3231 DRIVER"
    );

    Serial.println(
        "================================="
    );

    if (!RTCHAL::begin())
    {
        Serial.println(
            "ERREUR : DS3231 non detecte."
        );

        _ready = false;

        return false;
    }

    _ready = true;

    Serial.println(
        "DS3231 detecte !"
    );

    Serial.println(
        "DS3231 Driver : OK"
    );

    return true;
}

bool DS3231Driver::isReady() const
{
    return _ready;
}

bool DS3231Driver::readDateTime(
    RTCDateTime& dateTime
)
{
    if (!_ready)
    {
        return false;
    }

    uint8_t registers[7];

    if (
        !RTCHAL::readRegisters(
            0x00,
            registers,
            7
        )
    )
    {
        return false;
    }

    dateTime.second =
        bcdToDecimal(
            registers[0]
        );

    dateTime.minute =
        bcdToDecimal(
            registers[1]
        );

    dateTime.hour =
        bcdToDecimal(
            registers[2] & 0x3F
        );

    dateTime.day =
        bcdToDecimal(
            registers[4]
        );

    dateTime.month =
        bcdToDecimal(
            registers[5] & 0x1F
        );

    uint16_t year =
        bcdToDecimal(
            registers[6]
        );

    dateTime.year =
        2000 + year;

    return true;
}

bool DS3231Driver::setDateTime(
    const RTCDateTime& dateTime
)
{
    if (!_ready)
    {
        return false;
    }

    if (
        dateTime.year < 2000 ||
        dateTime.year > 2099 ||
        dateTime.month < 1 ||
        dateTime.month > 12 ||
        dateTime.day < 1 ||
        dateTime.day > 31 ||
        dateTime.hour > 23 ||
        dateTime.minute > 59 ||
        dateTime.second > 59
    )
    {
        return false;
    }

    Wire.beginTransmission(
        DS3231_I2C_ADDRESS
    );

    Wire.write(0x00);

    Wire.write(
        decimalToBCD(
            dateTime.second
        )
    );

    Wire.write(
        decimalToBCD(
            dateTime.minute
        )
    );

    Wire.write(
        decimalToBCD(
            dateTime.hour
        )
    );

    // Jour de la semaine : 1 par défaut
    Wire.write(1);

    Wire.write(
        decimalToBCD(
            dateTime.day
        )
    );

    Wire.write(
        decimalToBCD(
            dateTime.month
        )
    );

    Wire.write(
        decimalToBCD(
            dateTime.year - 2000
        )
    );

    return (
        Wire.endTransmission() == 0
    );
}

uint8_t DS3231Driver::bcdToDecimal(
    uint8_t value
)
{
    return (
        ((value >> 4) * 10)
        +
        (value & 0x0F)
    );
}

uint8_t DS3231Driver::decimalToBCD(
    uint8_t value
)
{
    return (
        ((value / 10) << 4)
        |
        (value % 10)
    );
}