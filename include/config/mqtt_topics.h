/**
 * @file    mqtt_topics.h
 * @brief   Chaînes de topics MQTT (Fortico).
 *
 * @details Périmètre strict (Sprint 0, §5) : uniquement les gabarits de
 *          topics. Séparé de mqtt_config.h afin de permettre une évolution du
 *          schéma de topics sans toucher aux paramètres de connexion.
 *          Chaque gabarit contient un espace réservé "%s" à remplacer par le
 *          vehicleId à l'exécution.
 *
 * @note    Source : Document 08 (MQTT Specification, §3).
 * @warning [ASSUMPTION] Nomenclature proposée par cohérence architecturale ;
 *          à valider avec l'équipe backend avant implémentation figée.
 */
#pragma once

namespace fortico {
namespace topics {

constexpr const char* TELEMETRY = "fleet/%s/telemetry";
constexpr const char* GPS = "fleet/%s/gps";
constexpr const char* FUEL = "fleet/%s/fuel";
constexpr const char* BATTERY = "fleet/%s/battery";

constexpr const char* RFID = "fleet/%s/rfid";
constexpr const char* ENGINE = "fleet/%s/engine";
constexpr const char* ALARM = "fleet/%s/alarm";
constexpr const char* STATUS = "fleet/%s/status";
constexpr const char* BLACKBOX = "fleet/%s/blackbox";

constexpr const char* CMD = "fleet/%s/cmd";
constexpr const char* CMD_ACK = "fleet/%s/cmd/ack";

/// Le LWT est publié sur le même topic status avec mqtt=false.
constexpr const char* LWT = STATUS;

}  // namespace topics
}  // namespace fortico