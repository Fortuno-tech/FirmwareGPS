#ifndef PN532_DRIVER_H
#define PN532_DRIVER_H

#include <Arduino.h>
#include <Adafruit_PN532.h>

namespace ForticoDrivers {

class PN532Driver
{
public:

    PN532Driver();

    bool begin();

    bool isReady() const;

    bool readUID(
        uint8_t* uid,
        uint8_t* uidLength,
        uint16_t timeoutMs
    );

private:

    Adafruit_PN532 _pn532;

    bool _ready;
};

} // namespace ForticoDrivers

#endif