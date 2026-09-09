#include "services/gps/gps_service.h"

#include "config/hardware_config.h"
#include "utils/default_values.h"
#include "utils/timer_utils.h"

namespace fortico {
namespace services {
namespace {

types::GpsData invalidPosition()
{
    return {0.0, 0.0, constants::INVALID_ALTITUDE, constants::INVALID_SPEED,
            0.0F, constants::INVALID_DOP, constants::INVALID_DOP,
            constants::INVALID_SATELLITE_COUNT, 0U, false,
            utils::Timer::millis()};
}

}  // namespace

GpsService::GpsService(interfaces::IGpsDriver& driver)
    : _driver(driver), _data(invalidPosition()), _lastPollMs(0U),
      _running(false)
{
}

bool GpsService::start()
{
    if (_running) return true;
    if (_driver.init() != constants::ErrorCode::OK || !_driver.start() ||
        !_driver.enableGnss()) {
        return false;
    }
    _data = invalidPosition();
    _lastPollMs = utils::Timer::millis();
    _running = true;
    return true;
}

void GpsService::stop()
{
    _driver.disableGnss();
    _driver.stop();
    _running = false;
}

bool GpsService::isRunning() const
{
    return _running && _driver.isHealthy();
}

void GpsService::update()
{
    if (!isRunning() ||
        !utils::Timer::timeout(_lastPollMs, hardware::GNSS_POLL_INTERVAL_MS)) {
        return;
    }
    _lastPollMs = utils::Timer::millis();
    types::GpsData position = invalidPosition();
    _driver.readPosition(position);
    _data = position;
}

const types::GpsData& GpsService::data() const { return _data; }

}  // namespace services
}  // namespace fortico
