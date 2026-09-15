#include "core/power/power_monitor.h"

#include "config/hardware_config.h"

namespace fortico {
namespace core {

PowerMonitor::PowerMonitor(drivers::BatteryDriver& battery)
	: _battery(battery), _state(PowerState::NORMAL), _initialized(false) {}

bool PowerMonitor::init()
{
	_initialized = _battery.isInitialized() ||
				   _battery.init() == constants::ErrorCode::OK;
	update();
	return _initialized;
}

void PowerMonitor::update()
{
	if (!_initialized) return;
	_battery.poll();
	float voltage = 0.0F;
	if (!_battery.readVoltage(voltage)) {
		_state = PowerState::CRITICAL;
	} else if (_battery.isCritical()) {
		_state = PowerState::CRITICAL;
	} else if (voltage < hardware::BATTERY_CRITICAL_VOLTAGE + 0.8F) {
		_state = PowerState::LOW_POWER;
	} else {
		_state = PowerState::NORMAL;
	}
}

PowerState PowerMonitor::state() const { return _state; }
bool PowerMonitor::shouldSleep() const { return _state == PowerState::CRITICAL; }

}  // namespace core
}  // namespace fortico
