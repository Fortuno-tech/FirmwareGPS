#ifndef SIM7600_DRIVER_H
#define SIM7600_DRIVER_H

#include <Arduino.h>

class SIM7600Driver
{
public:

    SIM7600Driver();

    bool begin();

    bool testAT();

    String sendCommand(
        const String& command,
        uint32_t timeout = 2000
    );

    bool isReady() const;

private:

    HardwareSerial* _serial;

    bool _ready;
};

#endif