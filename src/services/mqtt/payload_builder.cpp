#include "services/mqtt/payload_builder.h"

#include <cstdio>

namespace fortico {
namespace services {

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

}  // namespace services
}  // namespace fortico
