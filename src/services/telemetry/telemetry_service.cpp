#include "services/telemetry/telemetry_service.h"

#include <cstring>

#include "services/mqtt/payload_builder.h"
#include "utils/timer_utils.h"

namespace fortico {
namespace services {

TelemetryService::TelemetryService(managers::TelemetryManager& manager,
								   ITelemetrySink* sink,
								   const GpsService* gpsService)
	: _manager(manager), _sink(sink), _gpsService(gpsService), _payload{},
	  _running(false), _publishIntervalMs(5000U), _lastPublishMs(0U),
	  _activeTracking(false) {}

bool TelemetryService::start()
{
	_manager.init();
	_running = true;
	_lastPublishMs = 0U;
	return true;
}

void TelemetryService::stop()
{
	_running = false;
	_activeTracking = false;
}

bool TelemetryService::isRunning() const { return _running; }

void TelemetryService::setActiveTracking(bool active)
{
	_activeTracking = active;
}

bool TelemetryService::isActiveTracking() const
{
	return _activeTracking;
}

void TelemetryService::setPublishInterval(uint32_t intervalMs)
{
	_publishIntervalMs = intervalMs;
}

bool TelemetryService::forcePublish()
{
	if (!_running) return false;
	if (_gpsService != nullptr) _manager.setGps(_gpsService->data());
	_manager.update();
	if (!PayloadBuilder::telemetry(_manager.packet(), _payload, sizeof(_payload))) return false;
	if (_sink != nullptr) {
		const bool published = _sink->publish(_payload, std::strlen(_payload));
		if (published) {
			_lastPublishMs = utils::Timer::millis();
		}
		return published;
	}
	return false;
}

void TelemetryService::setRelay(types::RelayState relay)
{
	_manager.setRelay(relay);
}

void TelemetryService::setAuthentication(types::AuthenticationResult result)
{
	_manager.setAuthentication(result);
}

void TelemetryService::setFuel(const types::FuelLevel& fuel)
{
	_manager.setFuel(fuel);
}

void TelemetryService::update()
{
	if (!_running) return;
	if (_gpsService != nullptr) _manager.setGps(_gpsService->data());
	_manager.update();

	// Le suivi actif vers MQTT ne publie régulièrement que si le chauffeur est authentifié
	if (!_activeTracking) return;

	const uint64_t nowMs = utils::Timer::millis();
	if (nowMs - _lastPublishMs < _publishIntervalMs) return;

	if (!PayloadBuilder::telemetry(_manager.packet(), _payload, sizeof(_payload))) return;
	if (_sink != nullptr) {
		if (_sink->publish(_payload, std::strlen(_payload))) {
			_lastPublishMs = nowMs;
		}
	}
}

const char* TelemetryService::lastPayload() const { return _payload; }

}  // namespace services
}  // namespace fortico
