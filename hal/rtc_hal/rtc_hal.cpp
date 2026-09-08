#include "rtc_hal.h"

#include "i2c_hal.h"
#include "pin_config.h"

bool RTCHAL::begin()
{
    return ForticoHAL::I2CHAL::init()
        && ForticoHAL::I2CHAL::devicePresent(
            DS3231_I2C_ADDRESS
        );
}

bool RTCHAL::readRegisters(
    uint8_t address,
    uint8_t* buffer,
    size_t length
)
{
    TwoWire& wire = ForticoHAL::I2CHAL::getWire();

    wire.beginTransmission(
        DS3231_I2C_ADDRESS
    );
    wire.write(address);

    if (wire.endTransmission() != 0)
    {
        return false;
    }

    if (
        wire.requestFrom(
            DS3231_I2C_ADDRESS,
            length
        ) != length
    )
    {
        return false;
    }

    for (size_t index = 0; index < length; ++index)
    {
        buffer[index] = wire.read();
    }

    return true;
}

bool RTCHAL::writeRegister(
    uint8_t address,
    uint8_t value
)
{
    TwoWire& wire = ForticoHAL::I2CHAL::getWire();

    wire.beginTransmission(
        DS3231_I2C_ADDRESS
    );
    wire.write(address);
    wire.write(value);

    return wire.endTransmission() == 0;
}