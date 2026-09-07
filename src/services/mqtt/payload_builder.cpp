#include "services/mqtt/payload_builder.h"

#include <cstdio>

namespace fortico {
namespace services {

bool PayloadBuilder::telemetry(const types::TelemetryPacket& packet,
							   char* output, std::size_t outputSize)
{
	if (output == nullptr || outputSize == 0U) return false;
	const int written = std::snprintf(output, outputSize,
		"{\"seq\":%lu,\"timestamp\":%llu,\"gps\":{\"lat\":%.7f,\"lon\":%.7f,\"speed\":%.2f,\"fix\":%s},\"battery\":{\"voltage\":%.2f,\"percentage\":%.2f,\"critical\":%s},\"fuel\":{\"liters\":%.2f,\"percentage\":%.2f,\"anomaly\":%s},\"temperature\":%.2f,\"relay\":%u,\"auth\":%u}",
		static_cast<unsigned long>(packet.seq),
		static_cast<unsigned long long>(packet.timestamp),
		packet.gps.latitude, packet.gps.longitude, packet.gps.speedKmh,
		packet.gps.fixValid ? "true" : "false",
		packet.battery.voltage, packet.battery.percentage,
		packet.battery.isCritical ? "true" : "false",
		packet.fuel.liters, packet.fuel.percentage,
		packet.fuel.anomalyDetected ? "true" : "false",
		packet.temperatureC, static_cast<unsigned int>(packet.relay),
		static_cast<unsigned int>(packet.lastAuthResult));
	return written >= 0 && static_cast<std::size_t>(written) < outputSize;
}

}  // namespace services
}  // namespace fortico
