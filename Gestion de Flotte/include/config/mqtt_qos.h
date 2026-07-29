/**
 * @file    mqtt_qos.h
 * @brief   Niveaux de qualité de service (QoS) MQTT par catégorie de message
 *          — Fortico.
 *
 * @details Périmètre strict (arborescence finale, 9 fichiers de
 *          configuration) : uniquement les niveaux de QoS. Séparé de
 *          network_config.h (connexion au broker) et de mqtt_topics.h
 *          (chaînes de topics) pour permettre de faire évoluer la politique
 *          de QoS indépendamment du reste — ex. passer les alertes SOS en
 *          QoS 2 sans toucher à la configuration de connexion.
 *
 * @note    Source : Document 08 (MQTT Specification, §4).
 */
#pragma once

#include <cstdint>

namespace fortico {
namespace mqtt_qos {

/// QoS par défaut pour la télémétrie, l'authentification et le statut
constexpr uint8_t DEFAULT = 1;

/// QoS pour les alertes SOS (exactly-once, doublon inacceptable)
constexpr uint8_t SOS_ALERT = 2;

/// QoS des commandes entrantes (topic cmd)
constexpr uint8_t COMMAND = 1;

/// QoS du manifeste OTA (topic ota)
constexpr uint8_t OTA = 1;

/// QoS du Last Will and Testament
constexpr uint8_t LWT = 1;

/// QoS des alertes non-SOS (batterie faible, GPS perdu, etc.)
constexpr uint8_t ALERT_STANDARD = 1;

}  // namespace mqtt_qos
}  // namespace fortico