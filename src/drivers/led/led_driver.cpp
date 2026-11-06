#include "drivers/led/led_driver.h"

#include "config/pin_config.h"

namespace fortico {
namespace drivers {

LedDriver::LedDriver(hal::GpioHal& gpioHal)
	: _gpioHal(gpioHal), _initialized(false), _healthy(false),
	  _active(false), _enabled(false)
{
}

constants::ErrorCode LedDriver::init()
{
	if (_initialized) return constants::ErrorCode::OK;
	if (!_gpioHal.init(pins::LED_INDICATOR, hal::GpioMode::Output)) {
		return constants::ErrorCode::UNKNOWN;
	}
	_gpioHal.write(pins::LED_INDICATOR, false);
	_initialized = true;
	_healthy = true;
	_active = true;
	_enabled = false;
	return constants::ErrorCode::OK;
}

constants::ErrorCode LedDriver::poll()
{
	return _initialized ? constants::ErrorCode::OK
						: constants::ErrorCode::NOT_INITIALIZED;
}

bool LedDriver::start()
{
	if (!_initialized) return false;
	_active = true;
	_healthy = true;
	_gpioHal.write(pins::LED_INDICATOR, _enabled);
	return true;
}

bool LedDriver::stop()
{
	if (!_initialized) return false;
	_gpioHal.write(pins::LED_INDICATOR, false);
	_active = false;
	return true;
}

bool LedDriver::isHealthy() const { return _initialized && _healthy; }
bool LedDriver::isInitialized() const { return _initialized; }

void LedDriver::reset()
{
	_enabled = false;
	if (_initialized) {
		_gpioHal.write(pins::LED_INDICATOR, false);
		_active = true;
		_healthy = true;
	} else {
		_active = false;
		_healthy = false;
	}
}

bool LedDriver::set(bool enabled)
{
	if (!_initialized || !_active) return false;
	_gpioHal.write(pins::LED_INDICATOR, enabled);
	_enabled = enabled;
	return true;
}

bool LedDriver::on() { return set(true); }
bool LedDriver::off() { return set(false); }
bool LedDriver::toggle() { return set(!_enabled); }
bool LedDriver::isOn() const { return _enabled; }

}  // namespace drivers
}  // namespace fortico
