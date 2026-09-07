#include "managers/alert_manager/alert_manager.hpp"

#include "utils/timer_utils.h"

namespace fortico {
namespace managers {

AlertManager::AlertManager(drivers::ButtonDriver& button,
						   drivers::BuzzerDriver& buzzer,
						   drivers::LedDriver& led)
	: _button(button), _buzzer(buzzer), _led(led), _initialized(false),
	  _alertActive(false), _pending{}
{
}

void AlertManager::init()
{
	_button.init();
	_buzzer.init();
	_led.init();
	_initialized = true;
	clear();
}

void AlertManager::update()
{
	if (!_initialized) return;
	_button.poll();
	if (_button.wasPressed()) {
		raise(types::AlertCode::SOS_BUTTON, types::AlertSeverity::HIGH,
			  utils::Timer::millis());
	}
	if (_button.wasLongPressed()) {
		raise(types::AlertCode::SOS_BUTTON, types::AlertSeverity::HIGH,
			  utils::Timer::millis());
	}
}

void AlertManager::process()
{
	if (!_initialized || !_alertActive) return;
	_buzzer.on();
	_led.on();
}

void AlertManager::reset()
{
	_alertActive = false;
	_pending = {};
	_buzzer.off();
	_led.off();
}

bool AlertManager::raise(types::AlertCode code, types::AlertSeverity severity,
						 uint64_t timestamp)
{
	if (!_initialized) return false;
	_pending = {code, severity, timestamp};
	_alertActive = true;
	return true;
}

bool AlertManager::hasPendingAlert() const { return _alertActive; }
types::AlertEvent AlertManager::pendingAlert() const { return _pending; }

void AlertManager::clear()
{
	_alertActive = false;
	_pending = {};
	_buzzer.off();
	_led.off();
}

}  // namespace managers
}  // namespace fortico
