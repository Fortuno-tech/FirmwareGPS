#pragma once

#include <cstddef>

namespace fortico {
namespace services {

class MqttTopics final {
public:
    static bool build(const char* vehicleId, const char* channel,
                      char* output, std::size_t outputSize);

    static constexpr const char* TELEMETRY = "telemetry";
    static constexpr const char* GPS = "gps";
    static constexpr const char* FUEL = "fuel";
    static constexpr const char* BATTERY = "battery";
    static constexpr const char* RFID = "rfid";
    static constexpr const char* ENGINE = "engine";
    static constexpr const char* ALARM = "alarm";
    static constexpr const char* STATUS = "status";
    static constexpr const char* BLACKBOX = "blackbox";
    static constexpr const char* CMD = "cmd";
    static constexpr const char* CMD_ACK = "cmd/ack";
    static constexpr const char* LWT = "lwt";
};

}  // namespace services
}  // namespace fortico
