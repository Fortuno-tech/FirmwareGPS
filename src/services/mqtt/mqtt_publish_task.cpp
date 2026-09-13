#include "services/mqtt/mqtt_publish_task.h"

#include "config/task_config.h"
#include "utils/timer_utils.h"

namespace fortico {
namespace services {

MqttPublishTask::MqttPublishTask(MqttPublishQueue& queue,
                                 middleware::CellularMqttClient& client,
                                                                 core::Logger& logger,
                                                                 MqttCommandHandler& commandHandler)
        : _queue(queue), _client(client), _logger(logger),
            _commandHandler(commandHandler)
#ifdef ESP32_TARGET
      , _taskHandle(nullptr)
#endif
{
}

bool MqttPublishTask::start()
{
#ifdef ESP32_TARGET
    if (_taskHandle != nullptr) return false;
    return xTaskCreatePinnedToCore(
               &MqttPublishTask::taskEntry,
               "MqttPublishTask",
               tasks::MQTT_SERVICE_TASK_STACK_WORDS,
               this,
               tasks::MQTT_SERVICE_TASK_PRIORITY,
               &_taskHandle,
               tasks::CORE_NETWORK) == pdTRUE;
#else
    return false;
#endif
}

void MqttPublishTask::stop()
{
#ifdef ESP32_TARGET
    if (_taskHandle != nullptr) {
        vTaskDelete(_taskHandle);
        _taskHandle = nullptr;
    }
#endif
}

#ifdef ESP32_TARGET
void MqttPublishTask::taskEntry(void* context)
{
    static_cast<MqttPublishTask*>(context)->run();
}

void MqttPublishTask::run()
{
    for (;;) {
        drivers::MqttIncomingMessage incoming{};
        if (_client.pollIncoming(incoming)) {
            _logger.info(logging::TAG_MQTT,
                         "[MQTT][RX] message topic=%s payload_len=%u",
                         incoming.topic,
                         static_cast<unsigned int>(incoming.payloadLength));
            _commandHandler.handle(incoming);
        }

        MqttPublishMessage message{};
        if (!_queue.receive(message)) {
            drivers::MqttIncomingMessage retry{};
            if (_client.pollIncoming(retry)) {
                _logger.info(logging::TAG_MQTT,
                             "[MQTT][RX] message topic=%s payload_len=%u",
                             retry.topic,
                             static_cast<unsigned int>(retry.payloadLength));
                _commandHandler.handle(retry);
            }
            continue;
        }

        const uint64_t startMs = utils::Timer::millis();
        _logger.info(logging::TAG_MQTT,
                     "[PERF][MQTT_TASK] publish START topic=%s",
                     message.topic);
        const bool published = _client.publish(message.topic, message.payload,
                                               message.length);
        _logger.info(logging::TAG_MQTT,
                     "[PERF][MQTT_TASK] publish END duration=%llu result=%s",
                     static_cast<unsigned long long>(utils::Timer::millis() - startMs),
                     published ? "true" : "false");

        drivers::MqttIncomingMessage afterPublish{};
        if (_client.pollIncoming(afterPublish)) {
            _logger.info(logging::TAG_MQTT,
                         "[MQTT][RX] message topic=%s payload_len=%u",
                         afterPublish.topic,
                         static_cast<unsigned int>(afterPublish.payloadLength));
            _commandHandler.handle(afterPublish);
        }
    }
}
#endif

}  // namespace services
}  // namespace fortico
