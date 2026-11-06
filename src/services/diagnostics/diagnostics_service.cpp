#include "services/diagnostics/diagnostics_service.h"

#include "utils/timer_utils.h"

namespace fortico {
namespace services {

DiagnosticsService::DiagnosticsService(managers::DiagnosticsManager& manager,
									   core::Diagnostics& diagnostics,
									   interfaces::IPublisher* publisher)
	: _manager(manager), _diagnostics(diagnostics), _publisher(publisher),
	  _running(false) {}

bool DiagnosticsService::start()
{
	_diagnostics.init();
	_manager.init();
	_running = true;
	return true;
}

void DiagnosticsService::stop() { _running = false; }
bool DiagnosticsService::isRunning() const { return _running; }

void DiagnosticsService::update()
{
	if (!_running) return;
	_diagnostics.sample();
	const types::SystemHealth health = _diagnostics.health();
	_manager.setRuntimeMetrics(health.freeHeapBytes,
							   health.minFreeHeapBytes,
							   health.minStackWordsRelayTask,
							   health.i2cConsecutiveFailures,
							   health.sim7600ConsecutiveFailures);
	_manager.update();
	if (_publisher != nullptr && !_manager.allHealthy()) {
		_publisher->publish({constants::EventID::DIAGNOSTIC_WARNING,
							 utils::Timer::millis(), 0U});
	}
}

types::DiagnosticReport DiagnosticsService::report() const
{
	return _manager.getReport();
}

}  // namespace services
}  // namespace fortico
