#include "drivers/buzzer/buzzer_driver.h"

#include "config/pin_config.h"

namespace fortico {
namespace drivers {

BuzzerDriver::BuzzerDriver(hal::GpioHal& gpioHal)
	: _gpioHal(gpioHal), _initialized(false), _healthy(false),
	  _active(false), _enabled(false)
{
}

constants::ErrorCode BuzzerDriver::init()
{
	if (_initialized) {
		return constants::ErrorCode::OK;
	}
	if (!_gpioHal.init(pins::BUZZER_SIGNAL, hal::GpioMode::Output)) {
		return constants::ErrorCode::UNKNOWN;
	}
	_gpioHal.write(pins::BUZZER_SIGNAL, false);
	_initialized = true;
	_healthy = true;
	_active = true;
	_enabled = false;
	return constants::ErrorCode::OK;
}

constants::ErrorCode BuzzerDriver::poll()
{
	return _initialized ? constants::ErrorCode::OK
						: constants::ErrorCode::NOT_INITIALIZED;
}

bool BuzzerDriver::start()
{
	if (!_initialized) return false;
	_active = true;
	_healthy = true;
	return set(_enabled);
}

bool BuzzerDriver::stop()
{
	if (!_initialized) return false;
	_active = false;
	_gpioHal.write(pins::BUZZER_SIGNAL, false);
	return true;
}

bool BuzzerDriver::isHealthy() const { return _initialized && _healthy; }
bool BuzzerDriver::isInitialized() const { return _initialized; }

void BuzzerDriver::reset()
{
	_enabled = false;
	if (_initialized) {
		_gpioHal.write(pins::BUZZER_SIGNAL, false);
		_active = true;
		_healthy = true;
	} else {
		_active = false;
		_healthy = false;
	}
}

bool BuzzerDriver::set(bool enabled)
{
	if (!_initialized || !_active) return false;
	_gpioHal.write(pins::BUZZER_SIGNAL, enabled);
	_enabled = enabled;
	return true;
}

bool BuzzerDriver::on() { return set(true); }
bool BuzzerDriver::off() { return set(false); }
bool BuzzerDriver::isOn() const { return _enabled; }

}  // namespace drivers
}  // namespace fortico
