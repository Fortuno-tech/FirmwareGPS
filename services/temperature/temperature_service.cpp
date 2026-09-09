#include "temperature_service.h"
#include "../../include/config/hardware_config.h"

TemperatureService::TemperatureService(
    DHTDriver& driver
)
    : _driver(driver),
      _temperature(NAN),
      _humidity(NAN),
      _valid(false),
      _lastRead(0)
{
}

bool TemperatureService::begin()
{
    _valid = _driver.begin();

    if (_valid)
    {
        _temperature =
            _driver.getTemperature();

        _humidity =
            _driver.getHumidity();
    }

    _lastRead = millis();

    return _valid;
}

bool TemperatureService::update()
{
    unsigned long now = millis();

    if (
        now - _lastRead <
        DHT_INTERVAL_MS
    )
    {
        return _valid;
    }

    _lastRead = now;

    if (!_driver.read())
    {
        _valid = false;

        return false;
    }

    _temperature =
        _driver.getTemperature();

    _humidity =
        _driver.getHumidity();

    _valid = true;

    return true;
}

float TemperatureService::getTemperature() const
{
    return _temperature;
}

float TemperatureService::getHumidity() const
{
    return _humidity;
}

bool TemperatureService::isValid() const
{
    return _valid;
}