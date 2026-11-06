#pragma once

#include <cstring>
#include <string>

#include "middleware/mqtt_client/mqtt_client.h"
#include "services/mqtt/mqtt_topics.h"
#include "services/mqtt/telemetry_handoff.h"
#include "services/mqtt/telemetry_wire_payload.h"
#include "services/telemetry/itelemetry_sink.h"

namespace fortico {
namespace services {

class MqttTelemetrySink final : public ITelemetrySink {
public:
    MqttTelemetrySink(ITelemetryHandoff& handoff, const char* vehicleId)
        : _handoff(handoff), _vehicleId(vehicleId), _telemetryTopic{} {
        MqttTopics::build(vehicleId, MqttTopics::TELEMETRY,
                          _telemetryTopic, sizeof(_telemetryTopic));
    }

    bool begin() override { return _telemetryTopic[0] != '\0'; }

    bool publish(const char* payload, std::size_t length,
                 bool highPriority = false) override
    {
        (void)highPriority;
        return _handoff.enqueueTelemetry(_telemetryTopic, payload, length);
    }

    bool publishChannel(const char* channel, const char* payload,
                        std::size_t length, bool highPriority = false) override
    {
        (void)highPriority;
        char topic[96]{};
        return MqttTopics::build(_vehicleId, channel, topic, sizeof(topic)) &&
               _handoff.enqueueTelemetry(topic, payload, length);
    }

    const char* telemetryTopic() const { return _telemetryTopic; }

    bool publishRecord(middleware::MqttClient& client, uint64_t sequence,
                       const char* storedTopic, const uint8_t* payload,
                       std::size_t length)
    {
        const char* topic = storedTopic;
        if (topic == nullptr || topic[0] == '\0') {
            topic = _telemetryTopic;
        }
        if (topic == nullptr || topic[0] == '\0' ||
            std::strlen(topic) >= 96U) {
            return false;
        }
        std::string wirePayload;
        if (!TelemetryWirePayload::withOutboxSequence(
                payload, length, sequence, wirePayload)) {
            return false;
        }
        return client.publish(topic, wirePayload.data(), wirePayload.size());
    }

private:
    ITelemetryHandoff& _handoff;
    const char* _vehicleId;
    char _telemetryTopic[96];
};

}  // namespace services
}  // namespace fortico
