#pragma once

#include "core/logger/logger.h"
#include "middleware/mqtt_client/cellular_mqtt_client.h"
#include "services/mqtt/mqtt_publish_queue.h"
#include "services/mqtt/mqtt_command_handler.h"

#ifdef ESP32_TARGET
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

namespace fortico {
namespace services {

class MqttPublishTask final {
public:
    MqttPublishTask(MqttPublishQueue& queue,
                    middleware::CellularMqttClient& client,
                    core::Logger& logger,
                    MqttCommandHandler& commandHandler);

    bool start();
    void stop();

private:
#ifdef ESP32_TARGET
    static void taskEntry(void* context);
    void run();
#endif

    MqttPublishQueue& _queue;
    middleware::CellularMqttClient& _client;
    core::Logger& _logger;
    MqttCommandHandler& _commandHandler;
#ifdef ESP32_TARGET
    TaskHandle_t _taskHandle;
#endif
};

}  // namespace services
}  // namespace fortico
