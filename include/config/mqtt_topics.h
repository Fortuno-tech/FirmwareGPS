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
constexpr const char* TELEMETRY = "fortico/%s/telemetry";

/// Publication — événement d'authentification (badge lu, autorisé/refusé) (QoS 1)
constexpr const char* AUTH = "fortico/%s/auth";

/// Publication — alertes (SOS, anomalie) (QoS 1, ou 2 pour SOS — mqtt_config.h)
constexpr const char* ALERT = "fortico/%s/alert";

/// Publication — état de connexion, heartbeat, version firmware (QoS 1, retain)
constexpr const char* STATUS = "fortico/%s/status";

/// Souscription — commandes distantes (force-cut, unlock, ota-trigger) (QoS 1)
constexpr const char* CMD = "fortico/%s/cmd";

/// Souscription — métadonnées et manifeste de mise à jour OTA (QoS 1)
constexpr const char* OTA = "fortico/%s/ota";

/// Last Will and Testament — signalé hors-ligne en cas de déconnexion anormale
/// (QoS 1, retain)
constexpr const char* LWT = "fortico/%s/lwt";

}  // namespace topics
}  // namespace fortico