#ifndef SIM7600_SERVICE_H
#define SIM7600_SERVICE_H

#include <Arduino.h>

#include "sim7600_driver.h"

class SIM7600Service
{
public:

    SIM7600Service(
        SIM7600Driver& driver
    );

    bool begin();

    bool runDiagnostics();

    bool waitForNetwork();

    bool connectInternet();

    bool getIPAddress(
        String& ipAddress
    );

    bool isReady() const;

private:

    SIM7600Driver& _driver;

    bool _ready;
};

#endif