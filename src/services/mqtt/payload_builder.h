#pragma once

#include <cstddef>

#include "services/mqtt/mqtt_context.h"
#include "types/alert_types.h"
#include "types/telemetry_types.h"

namespace fortico {
namespace services {

class PayloadBuilder {
public:
	static bool telemetry(const types::TelemetryPacket& packet,
						  char* output, std::size_t outputSize);
	static bool telemetry(const types::TelemetryPacket& packet,
						  const char* deviceId, const char* vehicleId,
						  const char* timestampIso, char* output,
						  std::size_t outputSize);
	static bool telemetry(const types::TelemetryPacket& packet,
						  const MqttDeviceContext& context,
						  char* output, std::size_t outputSize);
	static bool gps(const types::TelemetryPacket& packet,
						const MqttDeviceContext& context,
						char* output, std::size_t outputSize);
	static bool fuel(const types::TelemetryPacket& packet,
						 const MqttDeviceContext& context,
						 char* output, std::size_t outputSize);
	static bool battery(const types::TelemetryPacket& packet,
						 const MqttDeviceContext& context,
						 char* output, std::size_t outputSize);
	static bool rfid(const char* uid, bool authorized,
						const MqttDeviceContext& context,
						char* output, std::size_t outputSize);
	static bool engine(types::RelayState relay, bool authorized,
						  const MqttDeviceContext& context,
						  char* output, std::size_t outputSize);
	static bool alarm(const types::AlertEvent& alert,
						 const MqttDeviceContext& context,
						 char* output, std::size_t outputSize);
	static bool status(const MqttDeviceContext& context, uint32_t uptime,
						uint32_t freeHeap, bool mqttConnected, bool simConnected,
						bool gpsConnected, bool sdMounted, bool temperatureValid,
						float boxTemperature, char* output, std::size_t outputSize);
	static bool commandAck(const MqttDeviceContext& context, const char* id,
						const char* command, bool success, const char* error,
						char* output, std::size_t outputSize);
};

}  // namespace services
}  // namespace fortico
