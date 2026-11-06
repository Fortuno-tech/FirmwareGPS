#include "services/mqtt/mqtt_command_handler.h"

#include <cstdio>
#include <cstring>

#include "app/system_controller/remote_relay_policy.hpp"
#include "app/system_controller/remote_relay_command_mailbox.hpp"
#include "config/safety_config.h"
#include "services/mqtt/mqtt_context.h"
#include "services/mqtt/mqtt_identity.h"
#include "services/mqtt/mqtt_topics.h"
#include "services/mqtt/payload_builder.h"
#include "utils/timer_utils.h"

namespace fortico {
namespace services {
namespace {

bool isWhitespace(char value)
{
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

void skipWhitespace(const char*& cursor, const char* end)
{
    while (cursor < end && isWhitespace(*cursor)) ++cursor;
}

bool readString(const char*& cursor, const char* end, char* output,
                std::size_t outputSize)
{
    if (cursor >= end || *cursor++ != '"' || output == nullptr ||
        outputSize == 0U) return false;
    std::size_t length = 0U;
    while (cursor < end && *cursor != '"') {
        const unsigned char value = static_cast<unsigned char>(*cursor++);
        if (value < 0x20U || value == '\\' || length + 1U >= outputSize) {
            return false;
        }
        output[length++] = static_cast<char>(value);
    }
    if (cursor >= end || *cursor++ != '"' || length == 0U) return false;
    output[length] = '\0';
    return true;
}

bool readUnsigned(const char*& cursor, const char* end, uint64_t& value)
{
    if (cursor >= end || *cursor < '0' || *cursor > '9') return false;
    value = 0U;
    do {
        const uint8_t digit = static_cast<uint8_t>(*cursor - '0');
        if (value > (UINT64_MAX - digit) / 10U) return false;
        value = value * 10U + digit;
        ++cursor;
    } while (cursor < end && *cursor >= '0' && *cursor <= '9');
    return true;
}

int hexValue(char value)
{
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

bool decodeMac(const char* hex, uint8_t* output)
{
    if (hex == nullptr || output == nullptr || std::strlen(hex) != 64U) {
        return false;
    }
    for (std::size_t index = 0U; index < 32U; ++index) {
        const int high = hexValue(hex[index * 2U]);
        const int low = hexValue(hex[index * 2U + 1U]);
        if (high < 0 || low < 0) return false;
        output[index] = static_cast<uint8_t>((high << 4) | low);
    }
    return true;
}

bool parseRequest(const char* payload, std::size_t length,
                  MqttCommandRequest& request)
{
    if (payload == nullptr || length == 0U || length > 512U) return false;
    const char* cursor = payload;
    const char* const end = payload + length;
    bool hasId = false;
    bool hasCommand = false;
    bool hasSequence = false;
    bool hasTimestamp = false;
    bool hasMac = false;
    char macHex[65]{};

    skipWhitespace(cursor, end);
    if (cursor >= end || *cursor++ != '{') return false;
    while (true) {
        skipWhitespace(cursor, end);
        char key[16]{};
        if (!readString(cursor, end, key, sizeof(key))) return false;
        skipWhitespace(cursor, end);
        if (cursor >= end || *cursor++ != ':') return false;
        skipWhitespace(cursor, end);
        if (std::strcmp(key, "id") == 0) {
            if (hasId || !readString(cursor, end, request.id,
                                     sizeof(request.id))) return false;
            hasId = true;
        } else if (std::strcmp(key, "command") == 0) {
            if (hasCommand || !readString(cursor, end, request.command,
                                          sizeof(request.command))) return false;
            hasCommand = true;
        } else if (std::strcmp(key, "seq") == 0) {
            if (hasSequence || !readUnsigned(cursor, end, request.sequence)) {
                return false;
            }
            hasSequence = true;
        } else if (std::strcmp(key, "timestamp") == 0) {
            if (hasTimestamp || !readUnsigned(cursor, end, request.timestamp)) {
                return false;
            }
            hasTimestamp = true;
        } else if (std::strcmp(key, "mac") == 0) {
            if (hasMac || !readString(cursor, end, macHex, sizeof(macHex))) {
                return false;
            }
            hasMac = true;
        } else {
            return false;
        }

        skipWhitespace(cursor, end);
        if (cursor >= end) return false;
        if (*cursor == '}') {
            ++cursor;
            break;
        }
        if (*cursor++ != ',') return false;
    }
    skipWhitespace(cursor, end);
    request.hasTimestamp = hasTimestamp;
    return cursor == end && hasId && hasCommand && hasSequence && hasMac &&
           decodeMac(macHex, request.mac);
}

}  // namespace

MqttCommandHandler::MqttCommandHandler(
    MqttPublishQueue& queue, managers::RelayManager& relayManager,
    TelemetryService& telemetryService, const GpsService& gpsService,
    const Sim7600Service& simService, const drivers::SdDriver& sdDriver,
    const middleware::CellularMqttClient& mqttClient,
    const drivers::RtcDriver& rtcDriver, core::Logger& logger,
    core::EventBus* eventBus)
    : _queue(queue), _relayManager(relayManager),
      _telemetryService(telemetryService), _gpsService(gpsService),
      _simService(simService), _sdDriver(sdDriver), _mqttClient(mqttClient),
      _rtcDriver(rtcDriver), _securityService(nullptr),
      _commandSecurity(nullptr), _storageService(nullptr),
      _authenticationService(nullptr), _logger(logger), _eventBus(eventBus)
{
}

MqttCommandHandler::MqttCommandHandler(
    MqttPublishQueue& queue, managers::RelayManager& relayManager,
    TelemetryService& telemetryService, const GpsService& gpsService,
    const Sim7600Service& simService, const drivers::SdDriver& sdDriver,
    const middleware::CellularMqttClient& mqttClient,
    const drivers::RtcDriver& rtcDriver, SecurityService& securityService,
    MqttCommandSecurity& commandSecurity, StorageService& storageService,
    AuthenticationService& authenticationService, core::Logger& logger,
    core::EventBus* eventBus)
    : _queue(queue), _relayManager(relayManager),
      _telemetryService(telemetryService), _gpsService(gpsService),
      _simService(simService), _sdDriver(sdDriver), _mqttClient(mqttClient),
      _rtcDriver(rtcDriver), _securityService(&securityService),
      _commandSecurity(&commandSecurity),
      _storageService(&storageService),
      _authenticationService(&authenticationService), _logger(logger),
      _eventBus(eventBus) {}

void MqttCommandHandler::setEventBus(core::EventBus* eventBus)
{
    _eventBus = eventBus;
}

bool MqttCommandHandler::queueReceiptAck(const char* id, const char* command,
                                         bool accepted, const char* error)
{
    MqttDeviceContext context{};
    if (!MqttContext::build(context, _rtcDriver)) {
        _logger.warn(logging::TAG_MQTT,
                     "[MQTT][RECEIPT_ACK] context build failed id=%s", id);
        return false;
    }
    char payload[768]{};
    if (!PayloadBuilder::commandAck(context, id, command, accepted, error,
                                    payload, sizeof(payload))) {
        _logger.warn(logging::TAG_MQTT,
                     "[MQTT][RECEIPT_ACK] payload build failed id=%s", id);
        return false;
    }
    char topic[96]{};
    if (!MqttTopics::build(MqttIdentity::vehicleId(), MqttTopics::CMD_ACK,
                           topic, sizeof(topic))) {
        _logger.warn(logging::TAG_MQTT,
                     "[MQTT][RECEIPT_ACK] topic build failed id=%s", id);
        return false;
    }
    const bool queued = _queue.enqueue(topic, payload, std::strlen(payload), true);
    if (queued) {
        _logger.info(logging::TAG_MQTT,
                     "[MQTT][RECEIPT_ACK] queued id=%s accepted=%s",
                     id, accepted ? "true" : "false");
    } else {
        _logger.warn(logging::TAG_MQTT,
                     "[MQTT][RECEIPT_ACK] queue rejected id=%s accepted=%s",
                     id, accepted ? "true" : "false");
    }
    return queued;
}

void MqttCommandHandler::audit(const MqttCommandRequest& request,
                               bool accepted, const char* reason,
                               uint64_t timestamp)
{
    const char* id = MqttCommandSecurity::validId(request.id)
        ? request.id : "";
    const char* command = MqttCommandSecurity::validCommand(request.command)
        ? request.command : "UNKNOWN";
    const char* detail = reason != nullptr ? reason : "ACCEPTED";
    if (_storageService == nullptr ||
        !_storageService->appendCommandAudit(
            id, command, request.sequence, accepted, detail, timestamp)) {
        _logger.warn(logging::TAG_MQTT,
                     "[MQTT][AUDIT] persisted audit unavailable id=%s command=%s reason=%s",
                     id, command, detail);
    }
    if (!accepted) {
        _logger.warn(logging::TAG_MQTT,
                     "[MQTT][CMD] rejected id=%s command=%s reason=%s",
                     id, command, detail);
    }
}

void MqttCommandHandler::handle(const drivers::MqttIncomingMessage& message)
{
    const uint32_t receivedAtMonotonicMs = message.receivedAtMonotonicMs;
    MqttCommandRequest request{};
    char expectedTopic[96]{};
    if (message.clientIndex != 0U ||
        !MqttTopics::build(MqttIdentity::vehicleId(), MqttTopics::CMD,
                           expectedTopic, sizeof(expectedTopic)) ||
        std::strcmp(message.topic, expectedTopic) != 0) {
        audit(request, false, "INVALID_TOPIC", _rtcDriver.timestamp());
        return;
    }

    if (!parseRequest(message.payload, message.payloadLength, request)) {
        audit(request, false, "INVALID_PAYLOAD", _rtcDriver.timestamp());
        queueReceiptAck("unknown", "UNKNOWN", false, "INVALID_PAYLOAD");
        return;
    }

    if (_securityService == nullptr || _commandSecurity == nullptr ||
        _storageService == nullptr ||
        _authenticationService == nullptr) {
        audit(request, false, "SECURITY_NOT_CONFIGURED",
              _rtcDriver.timestamp());
        queueReceiptAck(request.id, request.command, false,
                        "SECURITY_NOT_CONFIGURED");
        return;
    }

    char deviceId[64]{};
    const bool reliableClock = _rtcDriver.hasReliableTime();
    const uint64_t nowTimestamp = reliableClock ? _rtcDriver.timestamp() : 0U;
    if (!reliableClock) {
        _logger.warn(logging::TAG_MQTT,
                     "[MQTT][CMD] temporal validity window inactive; persistent sequence and ID state remains the anti-replay guard");
    }
    const bool activeAuthentication =
        _authenticationService->decision().state == types::AuthState::AUTHENTICATED;
    const char* rejectionReason = "DEVICE_ID_UNAVAILABLE";
    bool authenticated = _securityService->loadDeviceId(deviceId, sizeof(deviceId));
    if (authenticated) {
        authenticated = _commandSecurity->authorize(
            request, deviceId, reliableClock, nowTimestamp,
            activeAuthentication, rejectionReason);
    }

    bool accepted = false;
    const char* actionError = rejectionReason;
    const uint64_t eventTimestamp = reliableClock ? nowTimestamp : 0U;
    if (authenticated) {
        actionError = "UNKNOWN_COMMAND";
        if (std::strcmp(request.command, "STOP") == 0 ||
            std::strcmp(request.command, "ENGINE_OFF") == 0) {
            accepted = app::RemoteRelayCommandMailbox::instance().submit({
                types::RelayCmd::CUT, types::RelaySource::REMOTE_COMMAND,
                eventTimestamp}, request.id, request.command,
                receivedAtMonotonicMs);
            actionError = accepted ? nullptr : "ENGINE_OFF_REJECTED";
        } else if (std::strcmp(request.command, "KILL") == 0) {
            const types::GpsData& gps = _gpsService.data();
            const uint32_t nowMs = utils::Timer::millis();
            const bool safeToKill = app::remoteKillMayBeApplied(gps, nowMs);
            if (safeToKill) {
                accepted = app::RemoteRelayCommandMailbox::instance().submit({
                    types::RelayCmd::EMERGENCY_CUT,
                    types::RelaySource::REMOTE_COMMAND, eventTimestamp},
                    request.id, request.command, receivedAtMonotonicMs);
            }
            if (accepted) {
                _commandLatch.latchAfterKill();
                actionError = nullptr;
            } else {
                actionError = safeToKill ? "KILL_REJECTED" : "KILL_MOTION_UNSAFE";
            }
        } else if (std::strcmp(request.command, "SOS_ACK") == 0) {
            if (_eventBus != nullptr) {
                _eventBus->publish({constants::EventID::SOS_ACK_RECEIVED,
                                    eventTimestamp, 0U});
            }
            accepted = true;
            actionError = nullptr;
        } else if (std::strcmp(request.command, "ENGINE_ON") == 0) {
            if (_commandLatch.engineOnAllowed(activeAuthentication)) {
                accepted = app::RemoteRelayCommandMailbox::instance().submit({
                    types::RelayCmd::ENGAGE,
                    types::RelaySource::REMOTE_COMMAND, eventTimestamp},
                    request.id, request.command, receivedAtMonotonicMs);
            } else if (activeAuthentication) {
                actionError = "KILL_LATCHED";
            }
            if (accepted) actionError = nullptr;
            else if (actionError == nullptr) actionError = "ENGINE_ON_REJECTED";
        } else if (std::strcmp(request.command, "GET_STATUS") == 0) {
            accepted = _telemetryService.publishStatus(
                _mqttClient.connected(), _simService.isRunning(),
                _gpsService.isRunning(), _sdDriver.isHealthy());
            actionError = accepted ? nullptr : "STATUS_REJECTED";
        }
    }

    audit(request, accepted, actionError, eventTimestamp);
    queueReceiptAck(request.id, request.command, accepted, actionError);
}

}  // namespace services
}  // namespace fortico
