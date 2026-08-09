/**
 * @file    mqtt_events.h
 * @brief   Événements internes du cycle de vie MQTT (protocole), consommés
 *          par MqttService — Fortico.
 *
 * @details Distinct de constants/events.h (EventID) : mqtt_events.h couvre
 *          les événements de BAS NIVEAU du protocole MQTT lui-même
 *          (accusés de réception, échecs de publication), généralement
 *          internes à MqttService et non nécessairement republiés sur
 *          l'EventBus général. Document 08 (MQTT Specification) pour le
 *          contexte protocolaire complet.
 */
#pragma once

namespace fortico {
namespace constants {

enum class MqttEvent {
    CONNECTED,
    DISCONNECTED,
    SUBSCRIBED,
    UNSUBSCRIBED,
    PUBLISH_ACK,
    PUBLISH_FAILED,
    MESSAGE_RECEIVED,
    TLS_HANDSHAKE_FAILED,
};

}  // namespace constants
}  // namespace fortico
