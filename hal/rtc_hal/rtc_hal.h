#ifndef RTC_HAL_H
#define RTC_HAL_H

#include <Arduino.h>

class RTCHAL
{
public:

    static bool begin();

    static bool readRegisters(
        uint8_t address,
        uint8_t* buffer,
        size_t length
    );

    static bool writeRegister(
        uint8_t address,
        uint8_t value
    );
};

#endif