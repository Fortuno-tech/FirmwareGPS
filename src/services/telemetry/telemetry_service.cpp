#include "services/telemetry/telemetry_service.h"

#include <Arduino.h>
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
	const uint64_t publishStartMs = utils::Timer::millis();
	Serial.printf("[PERF][TELEMETRY] forcePublish START t=%llu\n",
			  static_cast<unsigned long long>(publishStartMs));
	if (!_running) return false;
	if (_gpsService != nullptr) _manager.setGps(_gpsService->data());
	_manager.update();
	if (!PayloadBuilder::telemetry(_manager.packet(), _payload, sizeof(_payload))) return false;
	Serial.printf("[PERF][TELEMETRY] payload build duration=%llu\n",
			  static_cast<unsigned long long>(utils::Timer::millis() - publishStartMs));
	if (_sink != nullptr) {
		Serial.printf("[PERF][MQTT] publish START t=%llu\n",
				  static_cast<unsigned long long>(utils::Timer::millis()));
		const bool published = _sink->publish(_payload, std::strlen(_payload), true);
		Serial.printf("[PERF][MQTT] publish END duration=%llu result=%s\n",
				  static_cast<unsigned long long>(utils::Timer::millis() - publishStartMs),
				  published ? "true" : "false");
		if (published) {
			_lastPublishMs = utils::Timer::millis();
		}
		Serial.printf("[PERF][TELEMETRY] forcePublish END duration=%llu result=%s\n",
				  static_cast<unsigned long long>(utils::Timer::millis() - publishStartMs),
				  published ? "true" : "false");
		return published;
	}
	Serial.printf("[PERF][TELEMETRY] forcePublish END duration=%llu result=false\n",
			  static_cast<unsigned long long>(utils::Timer::millis() - publishStartMs));
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
	if (_sink != nullptr &&
		_sink->publish(_payload, std::strlen(_payload), false)) {
		_lastPublishMs = nowMs;
	}
}

const char* TelemetryService::lastPayload() const { return _payload; }

}  // namespace services
}  // namespace fortico
