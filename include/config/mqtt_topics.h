/**
 * @file    mqtt_topics.h
 * @brief   Chaînes de topics MQTT (Fortico).
 *
 * @details Périmètre strict (Sprint 0, §5) : uniquement les gabarits de
 *          topics. Séparé de mqtt_config.h afin de permettre une évolution du
 *          schéma de topics sans toucher aux paramètres de connexion.
 *          Chaque gabarit contient un espace réservé "%s" à remplacer par le
 *          device_id à l'exécution (TelemetryService / MQTTService).
 *
 * @note    Source : Document 08 (MQTT Specification, §3).
 * @warning [ASSUMPTION] Nomenclature proposée par cohérence architecturale ;
 *          à valider avec l'équipe backend avant implémentation figée.
 */
#pragma once

namespace fortico {
namespace topics {

/// Publication — position, vitesse, capteurs, état relais (QoS 1)
constexpr const char* TELEMETRY = "fleet/%s/telemetry";

/// Publication — événement d'authentification (badge lu, autorisé/refusé) (QoS 1)
constexpr const char* AUTH = "fleet/%s/rfid";

/// Publication — alertes (SOS, anomalie) (QoS 1, ou 2 pour SOS — mqtt_config.h)
constexpr const char* ALERT = "fleet/%s/alarm";

/// Publication — état de connexion, heartbeat, version firmware (QoS 1, retain)
constexpr const char* STATUS = "fleet/%s/status";

/// Souscription — commandes distantes (force-cut, unlock, ota-trigger) (QoS 1)
constexpr const char* CMD = "fleet/%s/cmd";

/// Souscription — métadonnées et manifeste de mise à jour OTA (QoS 1)
constexpr const char* OTA = "fleet/%s/ota";

/// Last Will and Testament — signalé hors-ligne en cas de déconnexion anormale
/// (QoS 1, retain)
constexpr const char* LWT = "fleet/%s/lwt";

constexpr const char* GPS = "fleet/%s/gps";
constexpr const char* FUEL = "fleet/%s/fuel";
constexpr const char* BATTERY = "fleet/%s/battery";
constexpr const char* ENGINE = "fleet/%s/engine";
constexpr const char* BLACKBOX = "fleet/%s/blackbox";
constexpr const char* CMD_ACK = "fleet/%s/cmd/ack";

}  // namespace topics
}  // namespace fortico