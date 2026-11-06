#include "services/telemetry/telemetry_service.h"

#include <Arduino.h>
#include <cstring>
#include <cstdio>

#include "services/mqtt/payload_builder.h"
#include "services/mqtt/mqtt_timestamp.h"
#include "utils/timer_utils.h"

namespace fortico {
namespace services {

TelemetryService::TelemetryService(managers::TelemetryManager& manager,
								   ITelemetrySink* sink,
								   const GpsService* gpsService,
								   const drivers::RtcDriver* rtcDriver,
								   ITemperatureSensor* temperatureSensor)
	: _manager(manager), _sink(sink), _gpsService(gpsService),
	  _rtcDriver(rtcDriver), _temperatureSensor(temperatureSensor),
	  _mqttContext{}, _payload{}, _pendingPayload{},
	  _running(false), _hasPendingPayload(false),
	  _publishIntervalMs(5000U), _lastPublishMs(0U),
	  _lastEnqueueAttemptMs(0U),
	  _lastSlowPublishMs(0U),
	  _activeTracking(false) {}

bool TelemetryService::start()
{
	_manager.init();
	if (_sink != nullptr && !_sink->begin()) {
		Serial.println("[TELEMETRY][ERROR] telemetry handoff initialization failed");
		return false;
	}
	_running = true;
	_lastPublishMs = 0U;
	_lastEnqueueAttemptMs = 0U;
	_lastSlowPublishMs = 0U;
	_hasPendingPayload = false;
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
	if (!_hasPendingPayload) {
		if (_rtcDriver == nullptr ||
			!MqttContext::build(_mqttContext, *_rtcDriver) ||
			!PayloadBuilder::telemetry(_manager.packet(), _mqttContext,
										_pendingPayload, sizeof(_pendingPayload))) {
			return false;
		}
		_hasPendingPayload = true;
		std::strncpy(_payload, _pendingPayload, sizeof(_payload) - 1U);
	}
	if (_sink != nullptr) {
		const uint64_t enqueueStartMs = utils::Timer::millis();
		_lastEnqueueAttemptMs = enqueueStartMs;
		Serial.printf("[PERF][MQTT_BUILD] handoff START t=%llu\n",
				  static_cast<unsigned long long>(enqueueStartMs));
		const bool enqueued = _sink->publish(
			_pendingPayload, std::strlen(_pendingPayload), true);
		Serial.printf("[PERF][MQTT_BUILD] handoff END duration=%llu result=%s\n",
				  static_cast<unsigned long long>(utils::Timer::millis() - enqueueStartMs),
				  enqueued ? "true" : "false");
		if (enqueued) {
			_lastPublishMs = utils::Timer::millis();
			_hasPendingPayload = false;
		} else {
			Serial.println("[TELEMETRY][ERROR] handoff queue rejected payload; retaining for retry");
		}
		Serial.printf("[PERF][TELEMETRY] forcePublish END duration=%llu result=%s\n",
				  static_cast<unsigned long long>(utils::Timer::millis() - publishStartMs),
				  enqueued ? "true" : "false");
		return enqueued;
	}
	Serial.println("[TELEMETRY][ERROR] no telemetry sink; payload was not persisted");
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

bool TelemetryService::enqueueChannel(const char* channel, const char* payload,
									  std::size_t length, bool highPriority)
{
	const bool accepted = _sink != nullptr &&
		_sink->publishChannel(channel, payload, length, highPriority);
	if (!accepted) {
		Serial.printf("[TELEMETRY][ERROR] handoff queue rejected channel=%s\n",
				  channel == nullptr ? "(null)" : channel);
	}
	return accepted;
}

bool TelemetryService::publishRfid(const char* uid, bool authorized,
							   const drivers::RtcDateTime* eventTime)
{
	if (_sink == nullptr || _rtcDriver == nullptr ||
		!MqttContext::build(_mqttContext, *_rtcDriver)) return false;
	if (eventTime != nullptr && MqttTimestamp::format(
			*eventTime, _mqttContext.timestamp, sizeof(_mqttContext.timestamp))) {
		_mqttContext.timestampValid = true;
	}
	char payload[512]{};
	if (!PayloadBuilder::rfid(uid, authorized, _mqttContext,
							 payload, sizeof(payload))) return false;
	return enqueueChannel("rfid", payload, std::strlen(payload), true);
}

bool TelemetryService::publishEngine(types::RelayState relay, bool authorized)
{
	if (_sink == nullptr || _rtcDriver == nullptr ||
		!MqttContext::build(_mqttContext, *_rtcDriver)) return false;
	char payload[512]{};
	if (!PayloadBuilder::engine(relay, authorized, _mqttContext,
								payload, sizeof(payload))) return false;
	return enqueueChannel("engine", payload, std::strlen(payload), true);
}

bool TelemetryService::publishAlarm(const types::AlertEvent& alert)
{
	if (_sink == nullptr || _rtcDriver == nullptr ||
		!MqttContext::build(_mqttContext, *_rtcDriver)) return false;
	char payload[512]{};
	if (!PayloadBuilder::alarm(alert, _mqttContext,
							 payload, sizeof(payload))) return false;
	return enqueueChannel("alarm", payload, std::strlen(payload), true);
}

bool TelemetryService::publishStatus(bool mqttConnected, bool simConnected,
								 bool gpsConnected, bool sdMounted)
{
	if (_sink == nullptr || _rtcDriver == nullptr ||
		!MqttContext::build(_mqttContext, *_rtcDriver)) return false;
	float temperature = 0.0F;
	const bool temperatureValid = _temperatureSensor != nullptr &&
		_temperatureSensor->readCelsius(temperature);
	char payload[768]{};
	if (!PayloadBuilder::status(
			_mqttContext, millis() / 1000U, ESP.getFreeHeap(), mqttConnected,
			simConnected, gpsConnected, sdMounted, temperatureValid, temperature,
			payload, sizeof(payload))) return false;
	return enqueueChannel("status", payload, std::strlen(payload), false);
}

void TelemetryService::update()
{
	if (!_running) return;
	if (_gpsService != nullptr) _manager.setGps(_gpsService->data());
	_manager.update();

	// Le suivi actif ne génère pas de nouvelles mesures sans authentification,
	// mais un payload déjà retenu après erreur de stockage doit être réessayé.
	if (!_activeTracking && !_hasPendingPayload) return;

	const uint64_t nowMs = utils::Timer::millis();
	if (!_hasPendingPayload && nowMs - _lastPublishMs < _publishIntervalMs) return;
	if (_hasPendingPayload &&
		nowMs - _lastEnqueueAttemptMs < 1000U) return;

	if (!_hasPendingPayload) {
		if (_rtcDriver == nullptr ||
			!MqttContext::build(_mqttContext, *_rtcDriver) ||
			!PayloadBuilder::telemetry(_manager.packet(), _mqttContext,
										_pendingPayload, sizeof(_pendingPayload))) {
			return;
		}
		_hasPendingPayload = true;
		std::strncpy(_payload, _pendingPayload, sizeof(_payload) - 1U);
	}
	_lastEnqueueAttemptMs = nowMs;
	if (_sink == nullptr) {
		Serial.println("[TELEMETRY][ERROR] no telemetry sink; retaining payload");
		return;
	}
	if (!_sink->publish(_pendingPayload, std::strlen(_pendingPayload), false)) {
		Serial.println("[TELEMETRY][ERROR] handoff queue rejected payload; retaining for retry");
		return;
	}
	_hasPendingPayload = false;
	_lastPublishMs = nowMs;
	if (!_activeTracking) return;

	char auxiliaryPayload[1024]{};
	if (PayloadBuilder::gps(_manager.packet(), _mqttContext,
						 auxiliaryPayload, sizeof(auxiliaryPayload))) {
		enqueueChannel("gps", auxiliaryPayload,
					   std::strlen(auxiliaryPayload), false);
	}
	if (nowMs - _lastSlowPublishMs < 30000U) return;
	if (PayloadBuilder::fuel(_manager.packet(), _mqttContext,
						auxiliaryPayload, sizeof(auxiliaryPayload))) {
		enqueueChannel("fuel", auxiliaryPayload,
					   std::strlen(auxiliaryPayload), false);
	}
	if (PayloadBuilder::battery(_manager.packet(), _mqttContext,
							auxiliaryPayload, sizeof(auxiliaryPayload))) {
		enqueueChannel("battery", auxiliaryPayload,
					   std::strlen(auxiliaryPayload), false);
	}
	_lastSlowPublishMs = nowMs;
}

const char* TelemetryService::lastPayload() const { return _payload; }

}  // namespace services
}  // namespace fortico
