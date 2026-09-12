#pragma once

#include "services/mqtt/mqtt_publish_queue.h"
#include "services/telemetry/telemetry_service.h"

namespace fortico {
namespace services {

class MqttTelemetrySink final : public ITelemetrySink {
public:
    MqttTelemetrySink(MqttPublishQueue& queue, const char* topic)
        : _queue(queue), _topic(topic) {}

    bool publish(const char* payload, std::size_t length,
                 bool highPriority = false) override
    {
        return _queue.enqueue(_topic, payload, length, highPriority);
    }

private:
    MqttPublishQueue& _queue;
    const char* _topic;
};

}  // namespace services
}  // namespace fortico