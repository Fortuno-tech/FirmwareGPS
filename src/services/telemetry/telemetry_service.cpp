#include "services/telemetry/telemetry_service.h"

#include <Arduino.h>
#include <cstring>

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
	  _mqttContext{}, _payload{},
	  _running(false), _publishIntervalMs(5000U), _lastPublishMs(0U),
	  _lastSlowPublishMs(0U),
	  _activeTracking(false) {}

bool TelemetryService::start()
{
	_manager.init();
	_running = true;
	_lastPublishMs = 0U;
	_lastSlowPublishMs = 0U;
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
	if (_rtcDriver == nullptr || !MqttContext::build(_mqttContext, *_rtcDriver) ||
		!PayloadBuilder::telemetry(_manager.packet(), _mqttContext,
										_payload, sizeof(_payload))) return false;
	Serial.printf("[PERF][TELEMETRY] payload build duration=%llu\n",
			  static_cast<unsigned long long>(utils::Timer::millis() - publishStartMs));
	if (_sink != nullptr) {
		const uint64_t enqueueStartMs = utils::Timer::millis();
		Serial.printf("[PERF][MQTT_BUILD] enqueue START t=%llu\n",
				  static_cast<unsigned long long>(enqueueStartMs));
		const bool enqueued = _sink->publish(_payload, std::strlen(_payload), true);
		Serial.printf("[PERF][MQTT_BUILD] enqueue END duration=%llu result=%s\n",
				  static_cast<unsigned long long>(utils::Timer::millis() - enqueueStartMs),
				  enqueued ? "true" : "false");
		if (enqueued) {
			_lastPublishMs = utils::Timer::millis();
		}
		Serial.printf("[PERF][TELEMETRY] forcePublish END duration=%llu result=%s\n",
				  static_cast<unsigned long long>(utils::Timer::millis() - publishStartMs),
				  enqueued ? "true" : "false");
		return enqueued;
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
	return _sink->publishChannel("rfid", payload, std::strlen(payload), true);
}

bool TelemetryService::publishEngine(types::RelayState relay, bool authorized)
{
	if (_sink == nullptr || _rtcDriver == nullptr ||
		!MqttContext::build(_mqttContext, *_rtcDriver)) return false;
	char payload[512]{};
	if (!PayloadBuilder::engine(relay, authorized, _mqttContext,
								payload, sizeof(payload))) return false;
	return _sink->publishChannel("engine", payload, std::strlen(payload), true);
}

bool TelemetryService::publishAlarm(const types::AlertEvent& alert)
{
	if (_sink == nullptr || _rtcDriver == nullptr ||
		!MqttContext::build(_mqttContext, *_rtcDriver)) return false;
	char payload[512]{};
	if (!PayloadBuilder::alarm(alert, _mqttContext,
							 payload, sizeof(payload))) return false;
	return _sink->publishChannel("alarm", payload, std::strlen(payload), true);
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
	return _sink->publishChannel("status", payload, std::strlen(payload), false);
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

	if (_rtcDriver == nullptr || !MqttContext::build(_mqttContext, *_rtcDriver) ||
		!PayloadBuilder::telemetry(_manager.packet(), _mqttContext,
										_payload, sizeof(_payload))) return;
	if (_sink != nullptr &&
		_sink->publish(_payload, std::strlen(_payload), false)) {
		_lastPublishMs = nowMs;
	}
	if (_sink == nullptr) return;
	char auxiliaryPayload[1024]{};
	if (PayloadBuilder::gps(_manager.packet(), _mqttContext,
						 auxiliaryPayload, sizeof(auxiliaryPayload))) {
		_sink->publishChannel("gps", auxiliaryPayload,
						  std::strlen(auxiliaryPayload), false);
	}
	if (nowMs - _lastSlowPublishMs < 30000U) return;
	if (PayloadBuilder::fuel(_manager.packet(), _mqttContext,
						auxiliaryPayload, sizeof(auxiliaryPayload))) {
		_sink->publishChannel("fuel", auxiliaryPayload,
						  std::strlen(auxiliaryPayload), false);
	}
	if (PayloadBuilder::battery(_manager.packet(), _mqttContext,
							auxiliaryPayload, sizeof(auxiliaryPayload))) {
		_sink->publishChannel("battery", auxiliaryPayload,
						  std::strlen(auxiliaryPayload), false);
	}
	_lastSlowPublishMs = nowMs;
}

const char* TelemetryService::lastPayload() const { return _payload; }

}  // namespace services
}  // namespace fortico
