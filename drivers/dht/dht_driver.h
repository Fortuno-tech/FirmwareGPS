#ifndef DHT_DRIVER_H
#define DHT_DRIVER_H

#include <Arduino.h>
#include <DHT.h>

class DHTDriver
{
public:

    DHTDriver(
        uint8_t pin,
        uint8_t type
    );

    bool begin();

    bool read();

    float getTemperature() const;

    float getHumidity() const;

    bool isValid() const;

private:

    DHT _dht;

    float _temperature;

    float _humidity;

    bool _valid;
};

#endif