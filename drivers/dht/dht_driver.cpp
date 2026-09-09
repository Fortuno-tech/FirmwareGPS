#include "dht_driver.h"

DHTDriver::DHTDriver(
    uint8_t pin,
    uint8_t type
)
    : _dht(pin, type),
      _temperature(NAN),
      _humidity(NAN),
      _valid(false)
{
}

bool DHTDriver::begin()
{
    _dht.begin();

    delay(1000);

    return read();
}

bool DHTDriver::read()
{
    float humidity =
        _dht.readHumidity();

    float temperature =
        _dht.readTemperature();

    if (
        isnan(humidity) ||
        isnan(temperature)
    )
    {
        _valid = false;

        Serial.println(
            "[DHT] ERROR lecture"
        );

        return false;
    }

    _temperature = temperature;
    _humidity = humidity;

    _valid = true;

    Serial.printf(
        "[DHT] T=%.2f C | H=%.2f %%\n",
        _temperature,
        _humidity
    );

    return true;
}

float DHTDriver::getTemperature() const
{
    return _temperature;
}

float DHTDriver::getHumidity() const
{
    return _humidity;
}

bool DHTDriver::isValid() const
{
    return _valid;
}