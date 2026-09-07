#include "managers/power_manager/power_manager.h"

namespace fortico {
namespace managers {

PowerManager::PowerManager(core::PowerMonitor& monitor,
						   interfaces::IDriver* const* drivers,
						   std::size_t driverCount)
	: _monitor(monitor), _drivers(drivers), _driverCount(driverCount),
	  _initialized(false), _sleepRequested(false) {}

void PowerManager::init() { _initialized = _monitor.init(); update(); }
void PowerManager::update()
{
	if (!_initialized) return;
	_monitor.update();
	_sleepRequested = _monitor.shouldSleep();
	if (_sleepRequested) {
		for (std::size_t i = 0U; i < _driverCount; ++i)
			if (_drivers != nullptr && _drivers[i] != nullptr) _drivers[i]->stop();
	}
}
void PowerManager::process() { update(); }
void PowerManager::reset() { _sleepRequested = false; _monitor.init(); }
core::PowerState PowerManager::state() const { return _monitor.state(); }
bool PowerManager::isSleepRequested() const { return _sleepRequested; }

}  // namespace managers
}  // namespace fortico
