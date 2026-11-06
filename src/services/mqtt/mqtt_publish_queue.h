#pragma once

#include <cstddef>
#include <cstdint>

#include "core/logger/logger.h"
#include "services/mqtt/telemetry_handoff.h"

#ifdef ESP32_TARGET
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#endif

namespace fortico {
namespace services {

struct MqttPublishMessage {
    char topic[96];
    char payload[1024];
    std::size_t length;
};

class MqttPublishQueue final : public ITelemetryHandoff {
public:
    static constexpr std::size_t HighPriorityLength = 4U;
    static constexpr std::size_t NormalPriorityLength = 4U;
    static constexpr std::size_t TelemetryHandoffLength = 4U;

    explicit MqttPublishQueue(core::Logger& logger);

    bool enqueue(const char* topic, const void* payload, std::size_t length,
                 bool highPriority);
    bool receive(MqttPublishMessage& message);
    bool enqueueTelemetry(const char* topic, const void* payload,
                          std::size_t length) override;
    bool receiveTelemetry(MqttPublishMessage& message);

private:
    core::Logger& _logger;
#ifdef ESP32_TARGET
    QueueHandle_t _highPriorityQueue;
    QueueHandle_t _normalPriorityQueue;
    QueueHandle_t _telemetryQueue;
#endif
};

}  // namespace services
}  // namespace fortico
