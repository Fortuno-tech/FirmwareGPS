#pragma once

#include <cstddef>

#include "core/power/power_monitor.h"
#include "interfaces/imanager.h"
#include "interfaces/idriver.h"

namespace fortico {
namespace managers {

class PowerManager : public interfaces::IManager {
public:
	PowerManager(core::PowerMonitor& monitor, interfaces::IDriver* const* drivers,
				 std::size_t driverCount);
	void init() override;
	void update() override;
	void process() override;
	void reset() override;
	core::PowerState state() const;
	bool isSleepRequested() const;

private:
	core::PowerMonitor& _monitor;
	interfaces::IDriver* const* _drivers;
	std::size_t _driverCount;
	bool _initialized;
	bool _sleepRequested;
};

}  // namespace managers
}  // namespace fortico
