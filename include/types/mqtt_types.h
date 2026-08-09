/**
 * @file    mqtt_types.h
 * @brief   Types de connectivité réseau et MQTT — Fortico.
 *
 * @note    NetState formalise l'enum déjà mentionné au Document 07 §3
 *          (q_networkEvent : "enum NetState{REGISTERED, LOST, RECONNECTING}").
 */
#pragma once

#include <cstdint>
#include <cstddef>

namespace fortico {
namespace types {

/// État de connectivité réseau générique (NetworkService -> SystemController/MqttService)
enum class NetState {
    DISCONNECTED,
    REGISTERING,
    REGISTERED,
    LOST,
    RECONNECTING,
};

/// État de connexion MQTT (MqttService)
enum class MqttConnState {
    IDLE,
    TLS_HANDSHAKE,
    CONNECTING,
    CONNECTED,
    DISCONNECTED,
};

/// Message MQTT générique, utilisé en q_mqttTx/q_mqttRx (Document 07 §3)
struct MqttMessage {
    char topic[96];
    const uint8_t* payload;
    size_t payloadLen;
    uint8_t qos;
    bool retain;
};

}  // namespace types
}  // namespace fortico
