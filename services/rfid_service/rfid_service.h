#ifndef RFID_SERVICE_H
#define RFID_SERVICE_H

#include <Arduino.h>

#include "pn532_driver.h"

class RFIDService
{
public:

    RFIDService(
        ForticoDrivers::PN532Driver& driver
    );

    bool begin();

    bool update();

    bool isReady() const;

private:

    ForticoDrivers::PN532Driver& _driver;

    bool _ready;

    uint8_t _lastUid[7];
    uint8_t _uidLength;
};

#endif