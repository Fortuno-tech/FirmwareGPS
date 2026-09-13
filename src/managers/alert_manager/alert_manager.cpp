#include "managers/alert_manager/alert_manager.hpp"

#include "utils/timer_utils.h"

namespace fortico {
namespace managers {

AlertManager::AlertManager(drivers::ButtonDriver& button,
						   drivers::BuzzerDriver& buzzer,
						   drivers::LedDriver& led)
	: _button(button), _buzzer(buzzer), _led(led), _initialized(false),
	  _alertActive(false), _eventPending(false), _pendingEvent{}, _pending{},
	  _pattern(AlertPattern::OFF), _patternTimerMs(0U), _alarmStartTimeMs(0U),
	  _ledState(false)
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
	if (_button.wasPressed() || _button.wasLongPressed()) {
		raise(types::AlertCode::SOS_BUTTON, types::AlertSeverity::HIGH_SEVERITY,
			  utils::Timer::millis());
		setPattern(AlertPattern::SOS);
	}

	const uint64_t nowMs = utils::Timer::millis();
	switch (_pattern) {
		case AlertPattern::HALT:
		case AlertPattern::WAIT_BADGE:
			if (nowMs - _patternTimerMs >= 500U) {
				_patternTimerMs = nowMs;
				_ledState = !_ledState;
				_led.set(_ledState);
			}
			_buzzer.off();
			break;

		case AlertPattern::ERROR:
			_led.on();
			_buzzer.off();
			break;

		case AlertPattern::ALARM_FAST:
			if (nowMs - _patternTimerMs >= 100U) {
				_patternTimerMs = nowMs;
				_ledState = !_ledState;
				_led.set(_ledState);
			}
			if (nowMs - _alarmStartTimeMs < 60000U) {
				_buzzer.on();
			} else {
				_buzzer.off();
			}
			break;

		case AlertPattern::SOS:
			_led.on();
			_buzzer.on();
			break;

		case AlertPattern::TRACKING_OK:
		case AlertPattern::OFF:
		default:
			_led.off();
			_buzzer.off();
			break;
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
				setPattern(AlertPattern::ALARM_FAST);
				break;
			case constants::EventID::SOS_BUTTON_PRESSED:
				raise(types::AlertCode::SOS_BUTTON,
				      types::AlertSeverity::HIGH_SEVERITY, _pendingEvent.timestamp);
				setPattern(AlertPattern::SOS);
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
}

void AlertManager::reset()
{
	_alertActive = false;
	_eventPending = false;
	_pendingEvent = {};
	_pending = {};
	_pattern = AlertPattern::OFF;
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
	if (code == types::AlertCode::AUTH_DENIED) {
		setPattern(AlertPattern::ALARM_FAST);
	} else if (code == types::AlertCode::SOS_BUTTON) {
		setPattern(AlertPattern::SOS);
	}
	return true;
}

bool AlertManager::hasPendingAlert() const { return _alertActive; }
types::AlertEvent AlertManager::pendingAlert() const { return _pending; }

void AlertManager::clear()
{
	_alertActive = false;
	_pending = {};
	_pattern = AlertPattern::OFF;
	_buzzer.off();
	_led.off();
}

void AlertManager::setPattern(AlertPattern pattern)
{
	_pattern = pattern;
	_patternTimerMs = utils::Timer::millis();
	if (_pattern == AlertPattern::ALARM_FAST) {
		_alarmStartTimeMs = utils::Timer::millis();
	}
}

AlertPattern AlertManager::pattern() const
{
	return _pattern;
}

bool AlertManager::isAlarmTimedOut() const
{
	if (_pattern != AlertPattern::ALARM_FAST) return false;
	return (utils::Timer::millis() - _alarmStartTimeMs >= 60000U);
}

void AlertManager::stopAlarm()
{
	_alertActive = false;
	_pending = {};
	_buzzer.off();
	_led.off();
	setPattern(AlertPattern::WAIT_BADGE);
}

}  // namespace managers
}  // namespace fortico
