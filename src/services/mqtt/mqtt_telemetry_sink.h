#pragma once

#include "middleware/mqtt_client/mqtt_client.h"
#include "services/telemetry/telemetry_service.h"

namespace fortico {
namespace services {

class MqttTelemetrySink final : public ITelemetrySink {
public:
    MqttTelemetrySink(middleware::MqttClient& client, const char* topic)
        : _client(client), _topic(topic) {}

    bool publish(const char* payload, std::size_t length) override
    {
        return _client.publish(_topic, payload, length);
    }

private:
    middleware::MqttClient& _client;
    const char* _topic;
};

}  // namespace services
}  // namespace fortico