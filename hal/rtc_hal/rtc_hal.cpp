#include "rtc_hal.h"

#include <Wire.h>

#include "pin_config.h"

bool RTCHAL::begin()
{
    // Le bus I2C est déjà initialisé
    // par I2CHAL.

    Wire.beginTransmission(
        DS3231_I2C_ADDRESS
    );

    uint8_t error =
        Wire.endTransmission();

    return error == 0;
}

bool RTCHAL::readRegisters(
    uint8_t address,
    uint8_t* buffer,
    size_t length
)
{
    Wire.beginTransmission(
        DS3231_I2C_ADDRESS
    );

    Wire.write(address);

    if (
        Wire.endTransmission(
            false
        ) != 0
    )
    {
        return false;
    }

    uint8_t received =
        Wire.requestFrom(
            (uint8_t)DS3231_I2C_ADDRESS,
            (uint8_t)length
        );

    if (received != length)
    {
        return false;
    }

    for (
        size_t i = 0;
        i < length;
        i++
    )
    {
        buffer[i] = Wire.read();
    }

    return true;
}

bool RTCHAL::writeRegister(
    uint8_t address,
    uint8_t value
)
{
    Wire.beginTransmission(
        DS3231_I2C_ADDRESS
    );

    Wire.write(address);
    Wire.write(value);

    return (
        Wire.endTransmission() == 0
    );
}