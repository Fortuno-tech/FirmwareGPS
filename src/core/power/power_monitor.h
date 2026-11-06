#pragma once

#include "drivers/battery/battery_driver.h"

namespace fortico {
namespace core {

enum class PowerState { NORMAL, LOW_POWER, CRITICAL };

class PowerMonitor {
public:
	explicit PowerMonitor(drivers::BatteryDriver& battery);
	bool init();
	void update();
	PowerState state() const;
	bool shouldSleep() const;

private:
	drivers::BatteryDriver& _battery;
	PowerState _state;
	bool _initialized;
};

}  // namespace core
}  // namespace fortico
