#include "services/mqtt/mqtt_publish_task.h"

#include "config/task_config.h"
#include "app/connectivity_task/network_wait_policy.hpp"
#include "utils/timer_utils.h"

namespace fortico {
namespace services {

MqttPublishTask::MqttPublishTask(MqttPublishQueue& queue,
                                 TelemetryOutbox& outbox,
                                 middleware::CellularMqttClient& client,
                                                                 core::Logger& logger,
                                                                 MqttCommandHandler& commandHandler,
                                                                 MqttTelemetrySink& telemetrySink)
        : _queue(queue), _outbox(outbox), _client(client), _logger(logger),
            _commandHandler(commandHandler), _telemetrySink(telemetrySink),
            _watchdog(nullptr), _pendingTelemetry{}, _hasPendingTelemetry(false),
            _pendingGeneral{}, _hasPendingGeneral(false), _outboxReady(false),
            _lastOutboxAttemptMs(0U), _lastTelemetryReplayMs(0U),
            _lastGeneralPublishAttemptMs(0U)
#ifdef ESP32_TARGET
      , _taskHandle(nullptr), _startGate(xSemaphoreCreateBinary())
#endif
{
}

void MqttPublishTask::setWatchdog(core::Watchdog* watchdog) {
    _watchdog = watchdog;
}

bool MqttPublishTask::start()
{
#ifdef ESP32_TARGET
    if (_taskHandle != nullptr || _startGate == nullptr) return false;
    (void)xSemaphoreTake(_startGate, 0U);
    if (xTaskCreatePinnedToCore(
               &MqttPublishTask::taskEntry,
               "MqttPublishTask",
               tasks::MQTT_SERVICE_TASK_STACK_WORDS,
               this,
               tasks::MQTT_SERVICE_TASK_PRIORITY,
               &_taskHandle,
               tasks::CORE_NETWORK) != pdTRUE) {
        _taskHandle = nullptr;
        return false;
    }
    if (_watchdog != nullptr &&
        !_watchdog->registerTask(_taskHandle, "MqttPublishTask")) {
        vTaskDelete(_taskHandle);
        _taskHandle = nullptr;
        return false;
    }
    (void)xSemaphoreGive(_startGate);
    return true;
#else
    return false;
#endif
}

void MqttPublishTask::stop()
{
#ifdef ESP32_TARGET
    if (_taskHandle != nullptr) {
        const TaskHandle_t task = _taskHandle;
        if (_watchdog != nullptr) {
            (void)_watchdog->unregisterTask(task);
        }
        vTaskDelete(task);
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
    if (_startGate == nullptr ||
        xSemaphoreTake(_startGate,
            pdMS_TO_TICKS(
                fortico::app::network_wait::kWorkerStartupTimeoutMs)) != pdTRUE) {
        if (_watchdog != nullptr) {
            (void)_watchdog->unregisterTask(xTaskGetCurrentTaskHandle());
        }
        _taskHandle = nullptr;
        vTaskDelete(nullptr);
        return;
    }
    for (;;) {
        drivers::MqttIncomingMessage incoming{};
        if (_client.pollIncoming(incoming)) {
            _logger.info(logging::TAG_MQTT,
                         "[MQTT][RX] message topic=%s payload_len=%u",
                         incoming.topic,
                         static_cast<unsigned int>(incoming.payloadLength));
            _commandHandler.handle(incoming);
        }

        if (_watchdog != nullptr) {
            _watchdog->feed(_taskHandle);
        }

        const uint64_t nowMs = utils::Timer::millis();
        if (!_outboxReady && nowMs - _lastOutboxAttemptMs >= 1000U) {
            _lastOutboxAttemptMs = nowMs;
            const OutboxResult beginResult = _outbox.begin();
            _outboxReady = beginResult == OutboxResult::OK;
            if (!_outboxReady) {
                _logger.error(logging::TAG_MQTT,
                              "[TELEMETRY][ERROR] outbox initialization failed result=%u",
                              static_cast<unsigned int>(beginResult));
            }
        }

        if (!_hasPendingTelemetry &&
            _queue.receiveTelemetry(_pendingTelemetry)) {
            _hasPendingTelemetry = true;
        }
        if (_outboxReady && _hasPendingTelemetry &&
            nowMs - _lastOutboxAttemptMs >= 1000U) {
            _lastOutboxAttemptMs = nowMs;
            uint64_t sequence = 0U;
            const OutboxResult enqueueResult = _outbox.enqueue(
                _pendingTelemetry.topic,
                reinterpret_cast<const uint8_t*>(_pendingTelemetry.payload),
                _pendingTelemetry.length, sequence);
            if (enqueueResult == OutboxResult::OK) {
                _logger.info(logging::TAG_MQTT,
                             "[TELEMETRY][OUTBOX] persisted seq=%llu topic=%s",
                             static_cast<unsigned long long>(sequence),
                             _pendingTelemetry.topic);
                _hasPendingTelemetry = false;
                _pendingTelemetry = MqttPublishMessage{};
            } else {
                _logger.error(logging::TAG_MQTT,
                              "[TELEMETRY][ERROR] outbox enqueue failed result=%u; retaining handoff",
                              static_cast<unsigned int>(enqueueResult));
            }
        }

        if (_outboxReady && _client.connected() &&
            nowMs - _lastTelemetryReplayMs >= 100U) {
            _lastTelemetryReplayMs = nowMs;
            ReplayContext context{this};
            const OutboxResult replayResult = _outbox.publishNext(
                &MqttPublishTask::publishOutboxRecord, &context);
            if (replayResult != OutboxResult::OK &&
                replayResult != OutboxResult::EMPTY) {
                _logger.error(logging::TAG_MQTT,
                              "[MQTT][ERROR] durable telemetry replay failed result=%u",
                              static_cast<unsigned int>(replayResult));
            }
        }

        if (!_hasPendingGeneral &&
            !_queue.receive(_pendingGeneral)) {
            drivers::MqttIncomingMessage retry{};
            if (_client.pollIncoming(retry)) {
                _logger.info(logging::TAG_MQTT,
                             "[MQTT][RX] message topic=%s payload_len=%u",
                             retry.topic,
                             static_cast<unsigned int>(retry.payloadLength));
                _commandHandler.handle(retry);
            }
            vTaskDelay(pdMS_TO_TICKS(10U));
            continue;
        }

        _hasPendingGeneral = true;
        if (nowMs - _lastGeneralPublishAttemptMs < 1000U) {
            vTaskDelay(pdMS_TO_TICKS(10U));
            continue;
        }
        _lastGeneralPublishAttemptMs = nowMs;
        const uint64_t startMs = utils::Timer::millis();
        _logger.info(logging::TAG_MQTT,
                     "[PERF][MQTT_TASK] publish START topic=%s",
                     _pendingGeneral.topic);
        const bool published = _client.publish(
            _pendingGeneral.topic, _pendingGeneral.payload,
            _pendingGeneral.length);
        _logger.info(logging::TAG_MQTT,
                     "[PERF][MQTT_TASK] publish END duration=%llu result=%s",
                     static_cast<unsigned long long>(utils::Timer::millis() - startMs),
                     published ? "true" : "false");
        if (published) {
            _hasPendingGeneral = false;
            _pendingGeneral = MqttPublishMessage{};
        } else {
            _logger.error(logging::TAG_MQTT,
                          "[MQTT][ERROR] publish failed; retaining dequeued message topic=%s",
                          _pendingGeneral.topic);
        }

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

bool MqttPublishTask::publishOutboxRecord(void* opaque, uint64_t sequence,
                                          const char* topic,
                                          const uint8_t* payload,
                                          std::size_t length)
{
    ReplayContext& context = *static_cast<ReplayContext*>(opaque);
    return context.task != nullptr &&
           context.task->_telemetrySink.publishRecord(
               context.task->_client, sequence, topic, payload, length);
}

}  // namespace services
}  // namespace fortico
