#include "services/mqtt/mqtt_identity.h"

#include <Arduino.h>
#include <cstdio>

#include "config/project_config.h"

namespace fortico {
namespace services {

bool MqttIdentity::deviceId(char* output, std::size_t outputSize)
{
    if (output == nullptr || outputSize < 10U) return false;
    const uint64_t mac = ESP.getEfuseMac();
    const unsigned int suffix = static_cast<unsigned int>(mac & 0xFFFULL);
    const int written = std::snprintf(output, outputSize, "EV_DS6_%03X", suffix);
    return written > 0 && static_cast<std::size_t>(written) < outputSize;
}

const char* MqttIdentity::vehicleId() { return VehicleId; }

const char* MqttIdentity::firmwareVersion()
{
    return project::FIRMWARE_VERSION;
}

}  // namespace services
}  // namespace fortico
