#pragma once

#include <cstddef>
#include <cstdint>

namespace fortico {
namespace types {

struct MqttCredentials {
    char clientId[64];
    char username[64];
    char password[96];
};

/// État de connectivité réseau générique.
enum class NetState {
    DISCONNECTED,
    REGISTERING,
    REGISTERED,
    LOST,
    RECONNECTING,
};

/// État de connexion MQTT.
enum class MqttConnState {
    IDLE,
    TLS_HANDSHAKE,
    CONNECTING,
    CONNECTED,
    DISCONNECTED,
};

struct MqttMessage {
    char topic[96];
    const uint8_t* payload;
    std::size_t payloadLen;
    uint8_t qos;
    bool retain;
};

}  // namespace types
}  // namespace fortico
