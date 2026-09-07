#include "managers/alert_manager/alert_manager.hpp"

#include "utils/timer_utils.h"

namespace fortico {
namespace managers {

AlertManager::AlertManager(drivers::ButtonDriver& button,
						   drivers::BuzzerDriver& buzzer,
						   drivers::LedDriver& led)
	: _button(button), _buzzer(buzzer), _led(led), _initialized(false),
	  _alertActive(false), _eventPending(false), _pendingEvent{}, _pending{}
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
		raise(types::AlertCode::SOS_BUTTON, types::AlertSeverity::HIGH_SEVERITY,
			  utils::Timer::millis());
	}
	if (_button.wasLongPressed()) {
		raise(types::AlertCode::SOS_BUTTON, types::AlertSeverity::HIGH_SEVERITY,
			  utils::Timer::millis());
	}
}

void AlertManager::process()
{
	if (!_initialized) return;
	if (_eventPending) {
		switch (_pendingEvent.id) {
			case constants::EventID::AUTH_DENIED:
				raise(types::AlertCode::AUTH_DENIED, types::AlertSeverity::MEDIUM,
				      _pendingEvent.timestamp);
				break;
			case constants::EventID::SOS_BUTTON_PRESSED:
				raise(types::AlertCode::SOS_BUTTON,
				      types::AlertSeverity::HIGH_SEVERITY, _pendingEvent.timestamp);
				break;
			case constants::EventID::FUEL_ANOMALY:
				raise(types::AlertCode::FUEL_ANOMALY,
				      types::AlertSeverity::HIGH_SEVERITY, _pendingEvent.timestamp);
				break;
			case constants::EventID::RELAY_FAULT:
				raise(types::AlertCode::RELAY_FAULT,
				      types::AlertSeverity::HIGH_SEVERITY, _pendingEvent.timestamp);
				break;
			case constants::EventID::NETWORK_LOST:
				raise(types::AlertCode::NETWORK_LOST, types::AlertSeverity::MEDIUM,
				      _pendingEvent.timestamp);
				break;
			case constants::EventID::GPS_LOST:
				raise(types::AlertCode::GPS_LOST, types::AlertSeverity::MEDIUM,
				      _pendingEvent.timestamp);
				break;
			case constants::EventID::BATTERY_LOW:
				raise(types::AlertCode::BATTERY_LOW,
				      types::AlertSeverity::HIGH_SEVERITY, _pendingEvent.timestamp);
				break;
			case constants::EventID::DIAGNOSTIC_WARNING:
				raise(types::AlertCode::DIAGNOSTIC_WARNING,
				      types::AlertSeverity::MEDIUM, _pendingEvent.timestamp);
				break;
			default:
				break;
		}
		_eventPending = false;
	}
	if (!_alertActive) return;
	_buzzer.on();
	_led.on();
}

void AlertManager::reset()
{
	_alertActive = false;
	_eventPending = false;
	_pendingEvent = {};
	_pending = {};
	_buzzer.off();
	_led.off();
}

void AlertManager::onEvent(const types::Event& event)
{
	_eventPending = true;
	_pendingEvent = event;
}

void AlertManager::subscribeTo(interfaces::ISubscriber& eventBus)
{
	eventBus.subscribe(constants::EventID::AUTH_DENIED, this);
	eventBus.subscribe(constants::EventID::SOS_BUTTON_PRESSED, this);
	eventBus.subscribe(constants::EventID::FUEL_ANOMALY, this);
	eventBus.subscribe(constants::EventID::RELAY_FAULT, this);
	eventBus.subscribe(constants::EventID::NETWORK_LOST, this);
	eventBus.subscribe(constants::EventID::GPS_LOST, this);
	eventBus.subscribe(constants::EventID::BATTERY_LOW, this);
	eventBus.subscribe(constants::EventID::DIAGNOSTIC_WARNING, this);
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
