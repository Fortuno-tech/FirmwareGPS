#pragma once

#include <cstdint>

#include "core/logger/logger.h"
#include "core/watchdog/watchdog.h"
#include "middleware/mqtt_client/cellular_mqtt_client.h"
#include "services/mqtt/mqtt_publish_queue.h"
#include "services/mqtt/mqtt_command_handler.h"
#include "services/mqtt/mqtt_telemetry_sink.h"
#include "services/telemetry/telemetry_outbox.h"

#ifdef ESP32_TARGET
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#endif

namespace fortico {
namespace services {

class MqttPublishTask final {
public:
    MqttPublishTask(MqttPublishQueue& queue,
                    TelemetryOutbox& outbox,
                    middleware::CellularMqttClient& client,
                    core::Logger& logger,
                    MqttCommandHandler& commandHandler,
                    MqttTelemetrySink& telemetrySink);

    bool start();
    void stop();
    void setWatchdog(core::Watchdog* watchdog);
    TaskHandle_t taskHandle() const { return _taskHandle; }

private:
    struct ReplayContext {
        MqttPublishTask* task;
    };

    static bool publishOutboxRecord(void* context, uint64_t sequence,
                                    const char* topic, const uint8_t* payload,
                                    std::size_t length);

#ifdef ESP32_TARGET
    static void taskEntry(void* context);
    void run();
#endif

    MqttPublishQueue& _queue;
    TelemetryOutbox& _outbox;
    middleware::CellularMqttClient& _client;
    core::Logger& _logger;
    MqttCommandHandler& _commandHandler;
    MqttTelemetrySink& _telemetrySink;
    core::Watchdog* _watchdog;
    MqttPublishMessage _pendingTelemetry;
    bool _hasPendingTelemetry;
    MqttPublishMessage _pendingGeneral;
    bool _hasPendingGeneral;
    bool _outboxReady;
    uint64_t _lastOutboxAttemptMs;
    uint64_t _lastTelemetryReplayMs;
    uint64_t _lastGeneralPublishAttemptMs;
#ifdef ESP32_TARGET
    TaskHandle_t _taskHandle;
    SemaphoreHandle_t _startGate;
#endif
};

}  // namespace services
}  // namespace fortico
