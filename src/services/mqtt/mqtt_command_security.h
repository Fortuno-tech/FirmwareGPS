#pragma once

#include <cstddef>
#include <cstdint>

#include "interfaces/invs_driver.h"
#include "services/mqtt/command_mac_verifier.h"
#include "types/mqtt_types.h"

namespace fortico {
namespace services {

struct MqttCommandRequest {
    char id[64];
    char command[32];
    uint64_t sequence;
    uint64_t timestamp;
    bool hasTimestamp;
    uint8_t mac[32];
};

class MqttCommandSecurity final {
public:
    MqttCommandSecurity(interfaces::INvsDriver& nvsDriver,
                        const ICommandMacVerifier& macVerifier);

    bool authorize(const MqttCommandRequest& request, const char* deviceId,
                   bool reliableClock, uint64_t nowTimestamp,
                   bool activeLocalAuthentication,
                   const char*& rejectionReason);
    static bool buildCanonical(const MqttCommandRequest& request,
                               const char* deviceId, char* output,
                               std::size_t outputSize,
                               std::size_t& outputLength);
    static bool validId(const char* id);
    static bool validCommand(const char* command);

private:
    bool loadReplayState(types::MqttCommandReplayState& state) const;
    bool storeReplayState(const types::MqttCommandReplayState& state);

    interfaces::INvsDriver& _nvsDriver;
    const ICommandMacVerifier& _macVerifier;
};

class MqttCommandLatch final {
public:
    void latchAfterKill() { _killLatched = true; }
    bool engineOnAllowed(bool activeLocalAuthentication) const
    {
        return activeLocalAuthentication && !_killLatched;
    }

private:
    bool _killLatched = false;
};

}  // namespace services
}  // namespace fortico
