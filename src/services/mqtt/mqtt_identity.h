#pragma once

#include <cstddef>

namespace fortico {
namespace services {

class MqttIdentity final {
public:
    static constexpr const char* VehicleId = "VEH001";

    static bool deviceId(char* output, std::size_t outputSize);
    static const char* vehicleId();
    static const char* firmwareVersion();
};

}  // namespace services
}  // namespace fortico
