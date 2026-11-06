#include "managers/diagnostics_manager/diagnostics_manager.hpp"

#include "utils/timer_utils.h"

namespace fortico {
namespace managers {

DiagnosticsManager::DiagnosticsManager(interfaces::IDriver* const* drivers,
									   std::size_t driverCount)
	: _drivers(drivers), _driverCount(driverCount), _health{}, _report{},
	  _initialized(false)
{
}

void DiagnosticsManager::init()
{
	_initialized = true;
	update();
}

void DiagnosticsManager::update()
{
	if (!_initialized) return;
	for (std::size_t index = 0U; index < _driverCount; ++index) {
		if (_drivers != nullptr && _drivers[index] != nullptr) {
			_drivers[index]->poll();
		}
	}
	_health.heapWithinBudget = _health.freeHeapBytes > 0U &&
							   _health.minFreeHeapBytes > 0U;
	_report.health = _health;
	_report.reportTimestamp = utils::Timer::millis();
	_report.uptimeSeconds = static_cast<uint32_t>(
		_report.reportTimestamp / 1000U);
}

void DiagnosticsManager::process() { update(); }

void DiagnosticsManager::reset()
{
	for (std::size_t index = 0U; index < _driverCount; ++index) {
		if (_drivers != nullptr && _drivers[index] != nullptr) {
			_drivers[index]->reset();
		}
	}
	update();
}

types::SystemHealth DiagnosticsManager::getHealth() const { return _health; }
types::DiagnosticReport DiagnosticsManager::getReport() const { return _report; }

bool DiagnosticsManager::allHealthy() const
{
	if (!_initialized) return false;
	for (std::size_t index = 0U; index < _driverCount; ++index) {
		if (_drivers == nullptr || _drivers[index] == nullptr ||
			!_drivers[index]->isHealthy()) return false;
	}
	return _health.heapWithinBudget;
}

void DiagnosticsManager::setRuntimeMetrics(uint32_t freeHeapBytes,
											uint32_t minFreeHeapBytes,
											uint16_t minStackWordsRelayTask,
											uint8_t i2cFailures,
											uint8_t sim7600Failures)
{
	_health.freeHeapBytes = freeHeapBytes;
	_health.minFreeHeapBytes = minFreeHeapBytes;
	_health.minStackWordsRelayTask = minStackWordsRelayTask;
	_health.i2cConsecutiveFailures = i2cFailures;
	_health.sim7600ConsecutiveFailures = sim7600Failures;
}

}  // namespace managers
}  // namespace fortico
