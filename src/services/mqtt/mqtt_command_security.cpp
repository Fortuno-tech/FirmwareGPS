#include "services/mqtt/mqtt_command_security.h"

#include <cstdio>
#include <cstring>

#include "config/security_config.h"

namespace fortico {
namespace services {
namespace {

bool safeDeviceId(const char* value)
{
    if (value == nullptr || value[0] == '\0') return false;
    for (std::size_t index = 0U; index < 64U; ++index) {
        const char ch = value[index];
        if (ch == '\0') return true;
        if (!((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
              (ch >= '0' && ch <= '9') || ch == '-' || ch == '_')) {
            return false;
        }
    }
    return false;
}

bool validState(const types::MqttCommandReplayState& state)
{
    constexpr std::size_t historySize =
        sizeof(state.ids) / sizeof(state.ids[0]);
    if (state.idCount > historySize || state.nextIndex >= historySize) {
        return false;
    }
    if (state.sequence == 0U) {
        if (state.idCount != 0U || state.nextIndex != 0U) return false;
    } else if (state.idCount == 0U) {
        return false;
    }

    for (std::size_t index = 0U; index < historySize; ++index) {
        const bool occupied = state.idCount == historySize
            ? true : index < state.idCount;
        if (!occupied) {
            if (state.ids[index][0] != '\0') return false;
            continue;
        }
        if (std::memchr(state.ids[index], '\0',
                        sizeof(state.ids[index])) == nullptr ||
            !MqttCommandSecurity::validId(state.ids[index])) {
            return false;
        }
        for (std::size_t earlier = 0U; earlier < index; ++earlier) {
            if (std::strcmp(state.ids[index], state.ids[earlier]) == 0) {
                return false;
            }
        }
    }
    return state.idCount == historySize
        ? state.sequence > 0U
        : state.nextIndex == state.idCount;
}

}  // namespace

MqttCommandSecurity::MqttCommandSecurity(
    interfaces::INvsDriver& nvsDriver,
    const ICommandMacVerifier& macVerifier)
    : _nvsDriver(nvsDriver), _macVerifier(macVerifier)
{
}

bool MqttCommandSecurity::validId(const char* id)
{
    if (id == nullptr || id[0] == '\0') return false;
    for (std::size_t length = 0U; length < 64U; ++length) {
        const char ch = id[length];
        if (ch == '\0') return length > 0U;
        if (length >= 63U ||
            !((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
              (ch >= '0' && ch <= '9') || ch == '-' || ch == '_')) {
            return false;
        }
    }
    return false;
}

bool MqttCommandSecurity::validCommand(const char* command)
{
    return command != nullptr &&
        (std::strcmp(command, "STOP") == 0 ||
         std::strcmp(command, "ENGINE_OFF") == 0 ||
         std::strcmp(command, "KILL") == 0 ||
         std::strcmp(command, "ENGINE_ON") == 0 ||
         std::strcmp(command, "SOS_ACK") == 0 ||
         std::strcmp(command, "GET_STATUS") == 0);
}

bool MqttCommandSecurity::buildCanonical(
    const MqttCommandRequest& request, const char* deviceId, char* output,
    std::size_t outputSize, std::size_t& outputLength)
{
    outputLength = 0U;
    if (!validId(request.id) || !validCommand(request.command) ||
        request.sequence == 0U || !safeDeviceId(deviceId) ||
        output == nullptr || outputSize == 0U) return false;

    char timestamp[24]{};
    const char* timestampText = "-";
    if (request.hasTimestamp) {
        const int timestampLength = std::snprintf(
            timestamp, sizeof(timestamp), "%llu",
            static_cast<unsigned long long>(request.timestamp));
        if (timestampLength <= 0 ||
            static_cast<std::size_t>(timestampLength) >= sizeof(timestamp)) {
            return false;
        }
        timestampText = timestamp;
    }
    const int written = std::snprintf(
        output, outputSize, "FORTICO-CMD-V1\n%s\n%s\n%llu\n%s\n%s",
        deviceId, request.id,
        static_cast<unsigned long long>(request.sequence), request.command,
        timestampText);
    if (written <= 0 || static_cast<std::size_t>(written) >= outputSize) {
        return false;
    }
    outputLength = static_cast<std::size_t>(written);
    return true;
}

bool MqttCommandSecurity::loadReplayState(
    types::MqttCommandReplayState& state) const
{
    std::memset(&state, 0, sizeof(state));
    if (!_nvsDriver.open(security::NVS_NAMESPACE, true)) return false;
    const bool loaded = _nvsDriver.getBlob(
        security::NVS_KEY_MQTT_COMMAND_STATE, &state, sizeof(state));
    _nvsDriver.close();
    return loaded && validState(state);
}

bool MqttCommandSecurity::storeReplayState(
    const types::MqttCommandReplayState& state)
{
    if (!_nvsDriver.open(security::NVS_NAMESPACE, false)) return false;
    const bool stored = _nvsDriver.putBlob(
        security::NVS_KEY_MQTT_COMMAND_STATE, &state, sizeof(state));
    _nvsDriver.close();
    return stored;
}

bool MqttCommandSecurity::authorize(
    const MqttCommandRequest& request, const char* deviceId,
    bool reliableClock, uint64_t nowTimestamp,
    bool activeLocalAuthentication, const char*& rejectionReason)
{
    rejectionReason = "INVALID_COMMAND";
    if (!validId(request.id) || !validCommand(request.command) ||
        request.sequence == 0U) return false;

    if (reliableClock) {
        if (!request.hasTimestamp || nowTimestamp == 0U) {
            rejectionReason = "TIME_REQUIRED";
            return false;
        }
        const uint64_t delta = request.timestamp > nowTimestamp
            ? request.timestamp - nowTimestamp
            : nowTimestamp - request.timestamp;
        if (delta > security::COMMAND_VALIDITY_WINDOW_S) {
            rejectionReason = "EXPIRED";
            return false;
        }
    }

    char canonical[256]{};
    std::size_t canonicalLength = 0U;
    if (!buildCanonical(request, deviceId, canonical, sizeof(canonical),
                        canonicalLength) ||
        !_macVerifier.verifyCommandMac(
            reinterpret_cast<const uint8_t*>(canonical), canonicalLength,
            request.mac, sizeof(request.mac))) {
        rejectionReason = "BAD_MAC";
        return false;
    }

    types::MqttCommandReplayState previous{};
    if (!loadReplayState(previous)) {
        rejectionReason = "REPLAY_STATE_UNAVAILABLE";
        return false;
    }
    bool duplicateId = false;
    for (std::size_t index = 0U; index < previous.idCount; ++index) {
        if (std::strcmp(request.id, previous.ids[index]) == 0) {
            duplicateId = true;
            break;
        }
    }
    if (request.sequence <= previous.sequence || duplicateId) {
        rejectionReason = "REPLAY";
        return false;
    }

    types::MqttCommandReplayState next = previous;
    next.sequence = request.sequence;
    std::memset(next.ids[next.nextIndex], 0, sizeof(next.ids[next.nextIndex]));
    std::memcpy(next.ids[next.nextIndex], request.id,
                std::strlen(request.id) + 1U);
    if (next.idCount < sizeof(next.ids) / sizeof(next.ids[0])) {
        ++next.idCount;
    }
    next.nextIndex = static_cast<uint8_t>(
        (next.nextIndex + 1U) % (sizeof(next.ids) / sizeof(next.ids[0])));
    if (!storeReplayState(next)) {
        rejectionReason = "REPLAY_STATE_WRITE_FAILED";
        return false;
    }

    if (std::strcmp(request.command, "ENGINE_ON") == 0 &&
        !activeLocalAuthentication) {
        rejectionReason = "LOCAL_AUTH_REQUIRED";
        return false;
    }

    rejectionReason = nullptr;
    return true;
}

}  // namespace services
}  // namespace fortico
