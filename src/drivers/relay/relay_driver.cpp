#include "drivers/relay/relay_driver.h"

#include "config/pin_config.h"

namespace fortico {
namespace drivers {

RelayDriver::RelayDriver(hal::GpioHal& gpioHal)
	: _gpioHal(gpioHal), _initialized(false), _healthy(false),
	  _active(false), _engaged(false)
{
}

constants::ErrorCode RelayDriver::init()
{
	if (_initialized) return constants::ErrorCode::OK;
	if (!_gpioHal.init(pins::RELAY_CTRL, hal::GpioMode::Output)) {
		return constants::ErrorCode::RELAY_FAULT;
	}
	_gpioHal.write(pins::RELAY_CTRL, false);
	_initialized = true;
	_healthy = true;
	_active = true;
	_engaged = false;
	return constants::ErrorCode::OK;
}

constants::ErrorCode RelayDriver::poll()
{
	return _initialized ? constants::ErrorCode::OK
						: constants::ErrorCode::NOT_INITIALIZED;
}

bool RelayDriver::start()
{
	if (!_initialized) return false;
	_active = true;
	_healthy = true;
	_gpioHal.write(pins::RELAY_CTRL, _engaged);
	return true;
}

bool RelayDriver::stop()
{
	if (!_initialized) return false;
	_gpioHal.write(pins::RELAY_CTRL, false);
	_active = false;
	return true;
}

bool RelayDriver::isHealthy() const { return _initialized && _healthy; }
bool RelayDriver::isInitialized() const { return _initialized; }

void RelayDriver::reset()
{
	_engaged = false;
	if (_initialized) {
		_gpioHal.write(pins::RELAY_CTRL, false);
		_active = true;
		_healthy = true;
	} else {
		_active = false;
		_healthy = false;
	}
}

bool RelayDriver::set(bool engaged)
{
	if (!_initialized || !_active) return false;
	_gpioHal.write(pins::RELAY_CTRL, engaged);
	_engaged = engaged;
	return true;
}

bool RelayDriver::engage() { return set(true); }
bool RelayDriver::cut() { return set(false); }
bool RelayDriver::isEngaged() const { return _engaged; }

}  // namespace drivers
}  // namespace fortico
