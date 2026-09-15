#include "services/mqtt/payload_builder.h"

#include <cstdio>
#include <cstring>

#include "config/hardware_config.h"
#include "utils/timer_utils.h"

namespace fortico {
namespace services {

namespace {

struct EscapedContext {
	char deviceId[40];
	char vehicleId[40];
	char firmware[64];
};

bool escapeJson(const char* input, char* output, std::size_t outputSize)
{
	if (input == nullptr || output == nullptr || outputSize == 0U) return false;
	std::size_t written = 0U;
	for (std::size_t index = 0U; input[index] != '\0'; ++index) {
		const unsigned char character = static_cast<unsigned char>(input[index]);
		const char* escaped = nullptr;
		char unicode[7]{};
		switch (character) {
			case '"': escaped = "\\\""; break;
			case '\\': escaped = "\\\\"; break;
			case '\b': escaped = "\\b"; break;
			case '\f': escaped = "\\f"; break;
			case '\n': escaped = "\\n"; break;
			case '\r': escaped = "\\r"; break;
			case '\t': escaped = "\\t"; break;
			default: break;
		}
		if (escaped != nullptr) {
			const std::size_t length = std::strlen(escaped);
			if (written + length + 1U > outputSize) return false;
			std::memcpy(output + written, escaped, length);
			written += length;
		} else if (character < 0x20U) {
			std::snprintf(unicode, sizeof(unicode), "\\u%04X", character);
			if (written + 6U + 1U > outputSize) return false;
			std::memcpy(output + written, unicode, 6U);
			written += 6U;
		} else {
			if (written + 2U > outputSize) return false;
			output[written++] = static_cast<char>(character);
		}
	}
	output[written] = '\0';
	return true;
}

bool writeTimestamp(const MqttDeviceContext& context, char* output,
					   std::size_t outputSize)
{
	if (output == nullptr || outputSize == 0U) return false;
	if (!context.timestampValid) {
		return std::snprintf(output, outputSize, "null") > 0;
	}
	const int written = std::snprintf(output, outputSize, "\"%s\"",
									 context.timestamp);
	return written > 0 && static_cast<std::size_t>(written) < outputSize;
}

bool validContext(const MqttDeviceContext& context)
{
	return context.deviceId[0] != '\0' && context.vehicleId != nullptr &&
		context.firmware != nullptr;
}

bool prepareContext(const MqttDeviceContext& context, EscapedContext& output)
{
	return validContext(context) &&
		escapeJson(context.deviceId, output.deviceId, sizeof(output.deviceId)) &&
		escapeJson(context.vehicleId, output.vehicleId, sizeof(output.vehicleId)) &&
		escapeJson(context.firmware, output.firmware, sizeof(output.firmware));
}

}  // namespace

bool PayloadBuilder::telemetry(const types::TelemetryPacket& packet,
							   const char* deviceId, const char* vehicleId,
							   const char* timestampIso, char* output,
							   std::size_t outputSize)
{
	if (deviceId == nullptr || vehicleId == nullptr || timestampIso == nullptr ||
		output == nullptr || outputSize == 0U) return false;
	const int written = std::snprintf(output, outputSize,
		"{\"deviceId\":\"%s\",\"vehicleId\":\"%s\",\"timestamp\":\"%s\",\"seq\":%lu,\"gps\":{\"latitude\":%.7f,\"longitude\":%.7f,\"altitude\":%.1f,\"speed\":%.2f,\"heading\":%.1f,\"satellites\":%u,\"hdop\":%.2f,\"pdop\":%.2f,\"fix\":%s},\"fuel\":{\"liters\":%.2f,\"level\":%.2f,\"alarm\":%s},\"battery\":{\"voltage\":%.2f,\"percentage\":%.2f,\"critical\":%s},\"temperature\":null,\"relay\":%u,\"auth\":%u}",
		deviceId, vehicleId, timestampIso,
		static_cast<unsigned long>(packet.seq),
		packet.gps.latitude, packet.gps.longitude, packet.gps.altitudeMeters,
		packet.gps.speedKmh, packet.gps.headingDeg,
		static_cast<unsigned int>(packet.gps.satellites),
		packet.gps.hdop,
		packet.gps.pdop,
		packet.gps.fixValid ? "true" : "false",
		packet.fuel.liters, packet.fuel.percentage,
		packet.fuel.anomalyDetected ? "true" : "false",
		packet.battery.voltage, packet.battery.percentage,
		packet.battery.isCritical ? "true" : "false",
		static_cast<unsigned int>(packet.relay),
		static_cast<unsigned int>(packet.lastAuthResult));
	return written >= 0 && static_cast<std::size_t>(written) < outputSize;
}

bool PayloadBuilder::telemetry(const types::TelemetryPacket& packet,
							   const MqttDeviceContext& context,
							   char* output, std::size_t outputSize)
{
	EscapedContext escaped{};
	if (output == nullptr || outputSize == 0U || !prepareContext(context, escaped)) return false;
	char timestampJson[32]{};
	if (context.timestampValid) {
		const int timestampWritten = std::snprintf(
			timestampJson, sizeof(timestampJson), "\"%s\"", context.timestamp);
		if (timestampWritten < 0 ||
			static_cast<std::size_t>(timestampWritten) >= sizeof(timestampJson)) {
			return false;
		}
	} else {
		std::snprintf(timestampJson, sizeof(timestampJson), "null");
	}
	const int written = std::snprintf(
		output, outputSize,
		"{\"deviceId\":\"%s\",\"vehicleId\":\"%s\",\"timestamp\":%s,\"gps\":{\"latitude\":%.7f,\"longitude\":%.7f,\"altitude\":%.1f,\"speed\":%.2f,\"course\":%.1f,\"satellites\":%u,\"fix\":%s},\"fuel\":{\"liters\":%.2f,\"percentage\":%.2f,\"anomaly\":%s},\"battery\":{\"voltage\":%.2f,\"percentage\":%.2f,\"critical\":%s},\"rfid\":{\"driverId\":null,\"authorized\":%s},\"engine\":{\"relay\":%s,\"engineRunning\":null},\"network\":null,\"sdcard\":null,\"firmware\":\"%s\"}",
		escaped.deviceId, escaped.vehicleId,
		timestampJson,
		packet.gps.latitude, packet.gps.longitude, packet.gps.altitudeMeters,
		packet.gps.speedKmh, packet.gps.headingDeg,
		static_cast<unsigned int>(packet.gps.satellites),
		packet.gps.fixValid ? "true" : "false", packet.fuel.liters,
		packet.fuel.percentage, packet.fuel.anomalyDetected ? "true" : "false",
		packet.battery.voltage, packet.battery.percentage,
		packet.battery.isCritical ? "true" : "false",
		packet.lastAuthResult == types::AuthenticationResult::AUTHORIZED ? "true" : "false",
		packet.relay == types::RelayState::ENGAGED ? "true" : "false",
		escaped.firmware);
	return written >= 0 && static_cast<std::size_t>(written) < outputSize;
}

bool PayloadBuilder::gps(const types::TelemetryPacket& packet,
						 const MqttDeviceContext& context,
						 char* output, std::size_t outputSize)
{
	char timestamp[32]{};
	char fixAgeMs[24]{};
	EscapedContext escaped{};
	if (!prepareContext(context, escaped) || !writeTimestamp(context, timestamp, sizeof(timestamp))) return false;
	bool fixUsable = false;
	if (packet.gps.hasLastFix) {
		const uint32_t ageMs = utils::Timer::millis() -
			static_cast<uint32_t>(packet.gps.timestamp);
		std::snprintf(fixAgeMs, sizeof(fixAgeMs), "%lu",
				  static_cast<unsigned long>(ageMs));
		fixUsable = packet.gps.fixValid &&
			ageMs < hardware::GPS_LAST_FIX_MAX_AGE_MS;
	} else {
		std::snprintf(fixAgeMs, sizeof(fixAgeMs), "null");
	}
	const int written = std::snprintf(
		output, outputSize,
		"{\"deviceId\":\"%s\",\"vehicleId\":\"%s\",\"timestamp\":%s,\"latitude\":%.7f,\"longitude\":%.7f,\"altitude\":%.1f,\"speed\":%.2f,\"course\":%.1f,\"satellites\":%u,\"fix\":%s,\"fixAgeMs\":%s}",
		escaped.deviceId, escaped.vehicleId, timestamp, packet.gps.latitude,
		packet.gps.longitude, packet.gps.altitudeMeters, packet.gps.speedKmh,
		packet.gps.headingDeg, static_cast<unsigned int>(packet.gps.satellites),
		fixUsable ? "true" : "false", fixAgeMs);
	return written >= 0 && static_cast<std::size_t>(written) < outputSize;
}

bool PayloadBuilder::fuel(const types::TelemetryPacket& packet,
						  const MqttDeviceContext& context,
						  char* output, std::size_t outputSize)
{
	char timestamp[32]{};
	EscapedContext escaped{};
	if (!prepareContext(context, escaped) || !writeTimestamp(context, timestamp, sizeof(timestamp))) return false;
	const int written = std::snprintf(
		output, outputSize,
		"{\"deviceId\":\"%s\",\"vehicleId\":\"%s\",\"timestamp\":%s,\"liters\":%.2f,\"percentage\":%.2f,\"anomaly\":%s}",
		escaped.deviceId, escaped.vehicleId, timestamp, packet.fuel.liters,
		packet.fuel.percentage, packet.fuel.anomalyDetected ? "true" : "false");
	return written >= 0 && static_cast<std::size_t>(written) < outputSize;
}

bool PayloadBuilder::battery(const types::TelemetryPacket& packet,
							 const MqttDeviceContext& context,
							 char* output, std::size_t outputSize)
{
	char timestamp[32]{};
	EscapedContext escaped{};
	if (!prepareContext(context, escaped) || !writeTimestamp(context, timestamp, sizeof(timestamp))) return false;
	const int written = std::snprintf(
		output, outputSize,
		"{\"deviceId\":\"%s\",\"vehicleId\":\"%s\",\"timestamp\":%s,\"voltage\":%.2f,\"percentage\":%.2f,\"critical\":%s}",
		escaped.deviceId, escaped.vehicleId, timestamp, packet.battery.voltage,
		packet.battery.percentage, packet.battery.isCritical ? "true" : "false");
	return written >= 0 && static_cast<std::size_t>(written) < outputSize;
}

bool PayloadBuilder::rfid(const char* uid, bool authorized,
						  const MqttDeviceContext& context,
						  char* output, std::size_t outputSize)
{
	char timestamp[32]{};
	EscapedContext escaped{};
	char escapedUid[64]{};
	if (!prepareContext(context, escaped) || uid == nullptr ||
		!escapeJson(uid, escapedUid, sizeof(escapedUid)) ||
		!writeTimestamp(context, timestamp, sizeof(timestamp))) return false;
	const int written = std::snprintf(
		output, outputSize,
		"{\"deviceId\":\"%s\",\"vehicleId\":\"%s\",\"timestamp\":%s,\"uid\":\"%s\",\"authorized\":%s}",
		escaped.deviceId, escaped.vehicleId, timestamp, escapedUid,
		authorized ? "true" : "false");
	return written >= 0 && static_cast<std::size_t>(written) < outputSize;
}

bool PayloadBuilder::engine(types::RelayState relay, bool authorized,
							const MqttDeviceContext& context,
							char* output, std::size_t outputSize)
{
	char timestamp[32]{};
	EscapedContext escaped{};
	if (!prepareContext(context, escaped) || !writeTimestamp(context, timestamp, sizeof(timestamp))) return false;
	const int written = std::snprintf(
		output, outputSize,
		"{\"deviceId\":\"%s\",\"vehicleId\":\"%s\",\"timestamp\":%s,\"relay\":%s,\"engineRunning\":null,\"authorized\":%s}",
		escaped.deviceId, escaped.vehicleId, timestamp,
		relay == types::RelayState::ENGAGED ? "true" : "false",
		authorized ? "true" : "false");
	return written >= 0 && static_cast<std::size_t>(written) < outputSize;
}

bool PayloadBuilder::alarm(const types::AlertEvent& alert,
						   const MqttDeviceContext& context,
						   char* output, std::size_t outputSize)
{
	char timestamp[32]{};
	EscapedContext escaped{};
	if (!prepareContext(context, escaped) || !writeTimestamp(context, timestamp, sizeof(timestamp))) return false;
	const char* type = "UNKNOWN";
	switch (alert.code) {
		case types::AlertCode::AUTH_DENIED: type = "UNAUTHORIZED_DRIVER"; break;
		case types::AlertCode::SOS_BUTTON: type = "SOS"; break;
		case types::AlertCode::BATTERY_LOW: type = "LOW_BATTERY"; break;
		case types::AlertCode::FUEL_ANOMALY: type = "LOW_FUEL"; break;
		case types::AlertCode::GPS_LOST: type = "GPS_LOST"; break;
		case types::AlertCode::NETWORK_LOST: type = "NETWORK_LOST"; break;
		case types::AlertCode::RELAY_FAULT: type = "ENGINE_BLOCKED"; break;
		case types::AlertCode::DIAGNOSTIC_WARNING: type = "DIAGNOSTIC_WARNING"; break;
	}
	const char* severity = "LOW";
	if (alert.severity == types::AlertSeverity::MEDIUM) severity = "MEDIUM";
	if (alert.severity == types::AlertSeverity::HIGH_SEVERITY) severity = "HIGH";
	const int written = std::snprintf(
		output, outputSize,
		"{\"deviceId\":\"%s\",\"vehicleId\":\"%s\",\"timestamp\":%s,\"type\":\"%s\",\"severity\":\"%s\",\"active\":true}",
		escaped.deviceId, escaped.vehicleId, timestamp, type, severity);
	return written >= 0 && static_cast<std::size_t>(written) < outputSize;
}

bool PayloadBuilder::status(const MqttDeviceContext& context, uint32_t uptime,
							uint32_t freeHeap, bool mqttConnected, bool simConnected,
							bool gpsConnected, bool sdMounted, bool temperatureValid,
							float boxTemperature, char* output, std::size_t outputSize)
{
	char timestamp[32]{};
	EscapedContext escaped{};
	if (!prepareContext(context, escaped) || !writeTimestamp(context, timestamp, sizeof(timestamp))) return false;
	char temperature[24]{};
	if (temperatureValid) {
		std::snprintf(temperature, sizeof(temperature), "%.2f", boxTemperature);
	} else {
		std::snprintf(temperature, sizeof(temperature), "null");
	}
	const int written = std::snprintf(
		output, outputSize,
		"{\"deviceId\":\"%s\",\"vehicleId\":\"%s\",\"uptime\":%lu,\"freeHeap\":%lu,\"boxTemperature\":%s,\"wifi\":false,\"mqtt\":%s,\"sim\":\"%s\",\"gps\":\"%s\",\"sd\":\"%s\",\"timestamp\":%s,\"firmware\":\"%s\"}",
		escaped.deviceId, escaped.vehicleId,
		static_cast<unsigned long>(uptime), static_cast<unsigned long>(freeHeap),
		temperature,
		mqttConnected ? "true" : "false", simConnected ? "ServiceRunning" : "ServiceStopped",
		gpsConnected ? "ServiceRunning" : "ServiceStopped", sdMounted ? "Healthy" : "Unavailable",
		timestamp, escaped.firmware);
	return written >= 0 && static_cast<std::size_t>(written) < outputSize;
}

bool PayloadBuilder::commandAck(const MqttDeviceContext& context,
								const char* id, const char* command, bool success,
								const char* error, char* output,
								std::size_t outputSize)
{
	EscapedContext escaped{};
	char timestamp[32]{};
	char escapedId[80]{};
	char escapedCommand[48]{};
	char escapedError[80]{};
	if (!prepareContext(context, escaped) || id == nullptr || command == nullptr ||
		!escapeJson(id, escapedId, sizeof(escapedId)) ||
		!escapeJson(command, escapedCommand, sizeof(escapedCommand)) ||
		!writeTimestamp(context, timestamp, sizeof(timestamp))) return false;
	if (error == nullptr) std::snprintf(escapedError, sizeof(escapedError), "null");
	else if (!escapeJson(error, escapedError, sizeof(escapedError))) return false;
	char errorJson[96]{};
	if (error == nullptr) std::snprintf(errorJson, sizeof(errorJson), "null");
	else std::snprintf(errorJson, sizeof(errorJson), "\"%s\"", escapedError);
	const int written = std::snprintf(
		output, outputSize,
		"{\"id\":\"%s\",\"deviceId\":\"%s\",\"vehicleId\":\"%s\",\"timestamp\":%s,\"command\":\"%s\",\"success\":%s,\"error\":%s,\"firmware\":\"%s\"}",
		escapedId, escaped.deviceId, escaped.vehicleId, timestamp,
		escapedCommand, success ? "true" : "false", errorJson,
		escaped.firmware);
	return written >= 0 && static_cast<std::size_t>(written) < outputSize;
}

}  // namespace services
}  // namespace fortico
