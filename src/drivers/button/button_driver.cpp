#include "drivers/button/button_driver.h"

#include "config/hardware_config.h"
#include "config/pin_config.h"
#include "utils/timer_utils.h"

namespace fortico {
namespace drivers {

ButtonDriver::ButtonDriver(hal::GpioHal& gpioHal)
	: _gpioHal(gpioHal), _initialized(false), _healthy(false),
	  _active(false), _stablePressed(false), _candidatePressed(false),
	  _pressEvent(false), _longPressEvent(false),
	  _longPressReported(false), _candidateSinceMs(0U),
	  _pressedSinceMs(0U)
{
}

constants::ErrorCode ButtonDriver::init()
{
	if (_initialized) return constants::ErrorCode::OK;
	if (!_gpioHal.init(pins::BUTTON_SOS, hal::GpioMode::InputPullup)) {
		return constants::ErrorCode::UNKNOWN;
	}

	const bool pressed = !_gpioHal.read(pins::BUTTON_SOS);
	const uint32_t nowMs = utils::Timer::millis();
	_stablePressed = pressed;
	_candidatePressed = pressed;
	_candidateSinceMs = nowMs;
	_pressedSinceMs = nowMs;
	_pressEvent = false;
	_longPressEvent = false;
	_longPressReported = false;
	_initialized = true;
	_healthy = true;
	_active = true;
	return constants::ErrorCode::OK;
}

constants::ErrorCode ButtonDriver::poll()
{
	if (!_initialized) return constants::ErrorCode::NOT_INITIALIZED;
	if (!_active) return constants::ErrorCode::OK;

	const uint32_t nowMs = utils::Timer::millis();
	const bool pressed = !_gpioHal.read(pins::BUTTON_SOS);

	if (pressed != _candidatePressed) {
		_candidatePressed = pressed;
		_candidateSinceMs = nowMs;
	}

	if (_candidatePressed != _stablePressed &&
		utils::Timer::timeout(_candidateSinceMs, hardware::BUTTON_DEBOUNCE_MS)) {
		_stablePressed = _candidatePressed;
		if (_stablePressed) {
			_pressedSinceMs = nowMs;
			_longPressReported = false;
			_pressEvent = true;
		} else {
			_longPressReported = false;
		}
	}

	if (_stablePressed && !_longPressReported &&
		utils::Timer::timeout(_pressedSinceMs, hardware::BUTTON_LONG_PRESS_MS)) {
		_longPressReported = true;
		_longPressEvent = true;
	}

	return constants::ErrorCode::OK;
}

bool ButtonDriver::start()
{
	if (!_initialized) return false;
	_active = true;
	_healthy = true;
	return true;
}

bool ButtonDriver::stop()
{
	if (!_initialized) return false;
	_active = false;
	return true;
}

bool ButtonDriver::isHealthy() const { return _initialized && _healthy; }
bool ButtonDriver::isInitialized() const { return _initialized; }
bool ButtonDriver::isPressed() const { return _stablePressed; }

void ButtonDriver::reset()
{
	_pressEvent = false;
	_longPressEvent = false;
	_longPressReported = false;
	if (_initialized) {
		_active = true;
		_healthy = true;
	} else {
		_active = false;
		_healthy = false;
	}
}

bool ButtonDriver::wasPressed()
{
	const bool event = _pressEvent;
	_pressEvent = false;
	return event;
}

bool ButtonDriver::wasLongPressed()
{
	const bool event = _longPressEvent;
	_longPressEvent = false;
	return event;
}

}  // namespace drivers
}  // namespace fortico
