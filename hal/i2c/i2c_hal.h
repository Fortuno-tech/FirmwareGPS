// hal/i2c/i2c_hal.h
#ifndef I2C_HAL_H
#define I2C_HAL_H

#include <Arduino.h>
#include <Wire.h>
#include "config/pin_config.h"
#include "config/hardware_config.h"

namespace ForticoHAL {

class I2CHAL {
public:
    static bool init();
    static bool devicePresent(uint8_t address);
    static void scanBus();
    static TwoWire& getWire();

private:
    static bool _initialized;
};

} // namespace ForticoHAL

#endif // I2C_HAL_H