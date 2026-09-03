#include "i2c_hal.h"

#include "pin_config.h"
#include "hardware_config.h"

namespace ForticoHAL {

bool I2CHAL::_initialized = false;

bool I2CHAL::init()
{
    if (_initialized) {
        return true;
    }

    _initialized = Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQUENCY);
    if (_initialized) {
        Wire.setTimeOut(I2C_TIMEOUT_MS);
    }
    return _initialized;
}

bool I2CHAL::devicePresent(uint8_t address)
{
    if (!_initialized) {
        return false;
    }

    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
}

void I2CHAL::scanBus()
{
    if (!_initialized) {
        return;
    }

    for (uint8_t address = 1; address < 127; ++address) {
        if (devicePresent(address)) {
            Serial.printf("[I2C] Device trouve a 0x%02X\n", address);
        }
    }
}

TwoWire& I2CHAL::getWire()
{
    return Wire;
}

} // namespace ForticoHAL