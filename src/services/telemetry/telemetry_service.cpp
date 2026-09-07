#include "services/telemetry/telemetry_service.h"

#include <cstring>

#include "services/mqtt/payload_builder.h"

namespace fortico {
namespace services {

TelemetryService::TelemetryService(managers::TelemetryManager& manager,
								   ITelemetrySink* sink)
	: _manager(manager), _sink(sink), _payload{}, _running(false) {}

bool TelemetryService::start()
{
	_manager.init();
	_running = true;
	return true;
}

void TelemetryService::stop() { _running = false; }
bool TelemetryService::isRunning() const { return _running; }

void TelemetryService::update()
{
	if (!_running) return;
	_manager.update();
	if (!PayloadBuilder::telemetry(_manager.packet(), _payload, sizeof(_payload))) return;
	if (_sink != nullptr) _sink->publish(_payload, std::strlen(_payload));
}

const char* TelemetryService::lastPayload() const { return _payload; }

}  // namespace services
}  // namespace fortico
