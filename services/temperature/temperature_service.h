#ifndef TEMPERATURE_SERVICE_H
#define TEMPERATURE_SERVICE_H

#include <Arduino.h>
#include "dht_driver.h"

class TemperatureService
{
public:

    TemperatureService(
        DHTDriver& driver
    );

    bool begin();

    bool update();

    float getTemperature() const;

    float getHumidity() const;

    bool isValid() const;

private:

    DHTDriver& _driver;

    float _temperature;

    float _humidity;

    bool _valid;

    unsigned long _lastRead;
};

#endif