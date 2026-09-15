#pragma once

#include "services/mqtt/mqtt_publish_queue.h"
#include "services/mqtt/mqtt_topics.h"
#include "services/telemetry/telemetry_service.h"

namespace fortico {
namespace services {

class MqttTelemetrySink final : public ITelemetrySink {
public:
    MqttTelemetrySink(MqttPublishQueue& queue, const char* vehicleId)
        : _queue(queue), _vehicleId(vehicleId), _topic{} {
        MqttTopics::build(vehicleId, MqttTopics::TELEMETRY,
                          _topic, sizeof(_topic));
    }

    bool publish(const char* payload, std::size_t length,
                 bool highPriority = false) override
    {
        return _queue.enqueue(_topic, payload, length, highPriority);
    }

    bool publishChannel(const char* channel, const char* payload,
                        std::size_t length, bool highPriority = false) override
    {
        char topic[96]{};
        if (!MqttTopics::build(_vehicleId, channel, topic, sizeof(topic))) {
            return false;
        }
        return _queue.enqueue(topic, payload, length, highPriority);
    }

private:
    MqttPublishQueue& _queue;
    const char* _vehicleId;
    char _topic[96];
};

}  // namespace services
}  // namespace fortico