#include "services/mqtt/mqtt_topics.h"

#include <cstdio>
#include <cstring>

namespace fortico {
namespace services {

bool MqttTopics::build(const char* vehicleId, const char* channel,
                       char* output, std::size_t outputSize)
{
    if (vehicleId == nullptr || channel == nullptr || output == nullptr ||
        vehicleId[0] == '\0' || channel[0] == '\0' || outputSize == 0U) {
        return false;
    }
    if (std::strchr(vehicleId, '/') != nullptr ||
        std::strchr(channel, '/') != nullptr) {
        return false;
    }
    const int written = std::snprintf(output, outputSize, "fleet/%s/%s",
                                      vehicleId, channel);
    return written > 0 && static_cast<std::size_t>(written) < outputSize;
}

}  // namespace services
}  // namespace fortico
