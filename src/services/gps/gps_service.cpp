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
            constants::INVALID_SATELLITE_COUNT, 0U, false, false,
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
    if (!isRunning()) return;

    const uint32_t nowMs = utils::Timer::millis();
    if (_data.hasLastFix && _data.fixValid &&
        nowMs - static_cast<uint32_t>(_data.timestamp) >=
            hardware::GPS_LAST_FIX_MAX_AGE_MS) {
        _data.fixValid = false;
        _data.speedKmh = constants::INVALID_SPEED;
    }

    if (!utils::Timer::timeout(_lastPollMs, hardware::GNSS_POLL_INTERVAL_MS)) {
        return;
    }
    _lastPollMs = nowMs;
    types::GpsData position = invalidPosition();
    if (_driver.readPosition(position) && position.fixValid) {
        position.hasLastFix = true;
        _data = position;
    }
}

const types::GpsData& GpsService::data() const { return _data; }

}  // namespace services
}  // namespace fortico
