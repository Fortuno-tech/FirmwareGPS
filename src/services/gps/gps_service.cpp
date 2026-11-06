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
    if (_running.load(std::memory_order_acquire)) return true;
    if (_driver.init() != constants::ErrorCode::OK || !_driver.start() ||
        !_driver.enableGnss()) {
        return false;
    }
    setData(invalidPosition());
    _lastPollMs = utils::Timer::millis();
    _running.store(true, std::memory_order_release);
    return true;
}

void GpsService::stop()
{
    if (!_running.load(std::memory_order_acquire)) return;
    _driver.disableGnss();
    _driver.stop();
    _running.store(false, std::memory_order_release);
}

bool GpsService::isRunning() const
{
    return _running.load(std::memory_order_acquire) && _driver.isHealthy();
}

void GpsService::update()
{
    if (!isRunning()) return;

    const uint32_t nowMs = utils::Timer::millis();
    types::GpsData current = data();
    if (current.hasLastFix && current.fixValid &&
        nowMs - static_cast<uint32_t>(current.timestamp) >=
            hardware::GPS_LAST_FIX_MAX_AGE_MS) {
        current.fixValid = false;
        current.speedKmh = constants::INVALID_SPEED;
        setData(current);
    }

    if (!utils::Timer::timeout(_lastPollMs, hardware::GNSS_POLL_INTERVAL_MS)) {
        return;
    }
    _lastPollMs = nowMs;
    types::GpsData position = invalidPosition();
    if (_driver.readPosition(position) && position.fixValid) {
        position.hasLastFix = true;
        setData(position);
    }
}

types::GpsData GpsService::data() const
{
    portENTER_CRITICAL(&_dataMux);
    const types::GpsData snapshot = _data;
    portEXIT_CRITICAL(&_dataMux);
    return snapshot;
}

void GpsService::setData(const types::GpsData& data)
{
    portENTER_CRITICAL(&_dataMux);
    _data = data;
    portEXIT_CRITICAL(&_dataMux);
}

}  // namespace services
}  // namespace fortico
