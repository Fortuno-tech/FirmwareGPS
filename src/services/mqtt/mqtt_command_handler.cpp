#include "services/mqtt/mqtt_command_handler.h"

#include <cstdio>
#include <cstring>

#include "services/mqtt/mqtt_context.h"
#include "services/mqtt/mqtt_identity.h"
#include "services/mqtt/mqtt_topics.h"
#include "services/mqtt/payload_builder.h"

namespace fortico {
namespace services {

MqttCommandHandler::MqttCommandHandler(
    MqttPublishQueue& queue, managers::RelayManager& relayManager,
    TelemetryService& telemetryService, const GpsService& gpsService,
    const Sim7600Service& simService, const drivers::SdDriver& sdDriver,
    const middleware::CellularMqttClient& mqttClient,
    const drivers::RtcDriver& rtcDriver, core::Logger& logger)
    : _queue(queue), _relayManager(relayManager),
      _telemetryService(telemetryService), _gpsService(gpsService),
      _simService(simService), _sdDriver(sdDriver), _mqttClient(mqttClient),
      _rtcDriver(rtcDriver), _logger(logger) {}

bool MqttCommandHandler::extractString(const char* json, std::size_t length,
                                       const char* key, char* output,
                                       std::size_t outputSize) const
{
    if (json == nullptr || key == nullptr || output == nullptr ||
        outputSize < 2U) return false;
    char pattern[32]{};
    const int patternLength = std::snprintf(pattern, sizeof(pattern),
                                             "\"%s\"", key);
    if (patternLength <= 0 || static_cast<std::size_t>(patternLength) >= sizeof(pattern)) return false;
    const char* end = json + length;
    const char* found = std::strstr(json, pattern);
    if (found == nullptr || found >= end) return false;
    const char* cursor = found + patternLength;
    while (cursor < end && (*cursor == ' ' || *cursor == '\t' || *cursor == ':')) ++cursor;
    if (cursor >= end || *cursor != '"') return false;
    ++cursor;
    std::size_t outputLength = 0U;
    while (cursor < end && *cursor != '"') {
        if (*cursor == '\\' || outputLength + 1U >= outputSize) return false;
        output[outputLength++] = *cursor++;
    }
    if (cursor >= end || outputLength == 0U) return false;
    output[outputLength] = '\0';
    return true;
}

bool MqttCommandHandler::queueAck(const char* id, const char* command,
                                  bool success, const char* error)
{
    MqttDeviceContext context{};
    if (!MqttContext::build(context, _rtcDriver)) return false;
    char payload[768]{};
    if (!PayloadBuilder::commandAck(context, id, command, success, error,
                                    payload, sizeof(payload))) return false;
    char topic[96]{};
    if (!MqttTopics::build(MqttIdentity::vehicleId(), MqttTopics::CMD_ACK,
                           topic, sizeof(topic))) return false;
    const bool queued = _queue.enqueue(topic, payload, std::strlen(payload), true);
    _logger.info(logging::TAG_MQTT,
                 "[MQTT][ACK] queued id=%s success=%s",
                 id, success ? "true" : "false");
    return queued;
}

void MqttCommandHandler::handle(const drivers::MqttIncomingMessage& message)
{
    char expectedTopic[96]{};
    if (message.clientIndex != 0U ||
        !MqttTopics::build(MqttIdentity::vehicleId(), MqttTopics::CMD,
                           expectedTopic, sizeof(expectedTopic)) ||
        std::strcmp(message.topic, expectedTopic) != 0) return;

    char id[64]{};
    char command[32]{};
    const bool valid = extractString(message.payload, message.payloadLength,
                                     "id", id, sizeof(id)) &&
                       extractString(message.payload, message.payloadLength,
                                     "command", command, sizeof(command));
    if (!valid) {
        queueAck(id[0] != '\0' ? id : "unknown", command[0] != '\0' ? command : "UNKNOWN",
                 false, "INVALID_JSON");
        return;
    }
    _logger.info(logging::TAG_MQTT, "[MQTT][CMD] received id=%s command=%s", id, command);
    bool success = false;
    const char* error = "UNKNOWN_COMMAND";
    const uint64_t timestamp = _rtcDriver.timestamp();
    if (std::strcmp(command, "ENGINE_OFF") == 0) {
        success = _relayManager.submit({types::RelayCmd::CUT,
                                        types::RelaySource::REMOTE_COMMAND,
                                        timestamp});
        error = success ? nullptr : "ENGINE_OFF_REJECTED";
    } else if (std::strcmp(command, "ENGINE_ON") == 0) {
        success = _relayManager.submit({types::RelayCmd::ENGAGE,
                                        types::RelaySource::REMOTE_COMMAND,
                                        timestamp});
        error = success ? nullptr : "ENGINE_ON_REJECTED";
    } else if (std::strcmp(command, "GET_STATUS") == 0) {
        success = _telemetryService.publishStatus(
            _mqttClient.connected(), _simService.isRunning(),
            _gpsService.isRunning(), _sdDriver.isHealthy());
        error = success ? nullptr : "STATUS_REJECTED";
    }
    _logger.info(logging::TAG_MQTT, "[MQTT][CMD] %s %s",
                 command, success ? "accepted" : "rejected");
    queueAck(id, command, success, error);
}

}  // namespace services
}  // namespace fortico
