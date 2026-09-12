#include "services/mqtt/mqtt_publish_queue.h"

#include <cstring>

namespace fortico {
namespace services {

MqttPublishQueue::MqttPublishQueue(core::Logger& logger)
    : _logger(logger)
#ifdef ESP32_TARGET
      , _highPriorityQueue(xQueueCreate(HighPriorityLength,
                                         sizeof(MqttPublishMessage)))
      , _normalPriorityQueue(xQueueCreate(NormalPriorityLength,
                                           sizeof(MqttPublishMessage)))
#endif
{
}

bool MqttPublishQueue::enqueue(const char* topic, const void* payload,
                               std::size_t length, bool highPriority)
{
    if (topic == nullptr || payload == nullptr || topic[0] == '\0' ||
        length == 0U || std::strlen(topic) >= 96U || length > 1024U) {
        _logger.warn(logging::TAG_MQTT,
                     "[PERF][MQTT_QUEUE] DROP invalid message length=%u",
                     static_cast<unsigned int>(length));
        return false;
    }

#ifdef ESP32_TARGET
    MqttPublishMessage message{};
    std::strncpy(message.topic, topic, sizeof(message.topic) - 1U);
    std::memcpy(message.payload, payload, length);
    message.length = length;
    QueueHandle_t queue = highPriority ? _highPriorityQueue : _normalPriorityQueue;
    if (queue == nullptr || xQueueSend(queue, &message, 0U) != pdTRUE) {
        _logger.warn(logging::TAG_MQTT,
                     "[PERF][MQTT_QUEUE] DROP priority=%s",
                     highPriority ? "HIGH" : "NORMAL");
        return false;
    }
    _logger.info(logging::TAG_MQTT,
                 "[PERF][MQTT_QUEUE] enqueue priority=%s length=%u",
                 highPriority ? "HIGH" : "NORMAL",
                 static_cast<unsigned int>(length));
    return true;
#else
    (void)highPriority;
    return false;
#endif
}

bool MqttPublishQueue::receive(MqttPublishMessage& message)
{
#ifdef ESP32_TARGET
    if (_highPriorityQueue != nullptr &&
        xQueueReceive(_highPriorityQueue, &message, 0U) == pdTRUE) {
        return true;
    }
    if (_normalPriorityQueue != nullptr &&
        xQueueReceive(_normalPriorityQueue, &message, 0U) == pdTRUE) {
        return true;
    }
    vTaskDelay(pdMS_TO_TICKS(10U));
    return false;
#else
    (void)message;
    return false;
#endif
}

}  // namespace services
}  // namespace fortico
