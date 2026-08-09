/**
 * @file    network_config.h
 * @brief   Paramètres réseau génériques (cellulaire, FSM NetworkService,
 *          reconnexion) ET paramètres de connexion au broker MQTT — Fortico.
 *
 * @details Périmètre (révisé — arborescence finale, 9 fichiers de
 *          configuration, board_config.h retiré) : paramètres de
 *          connectivité réseau/modem ET connexion broker MQTT (hôte, port,
 *          keepalive). Les topics vivent dans mqtt_topics.h ; les niveaux de
 *          QoS dans mqtt_qos.h ; les clés/certificats dans
 *          security_config.h.
 *
 * @note    Source : Document 05 (Service Development Guide, §2 — FSM modem),
 *          Document 08 (MQTT Specification), SOP §3.1 (R-01 — zéro boucle
 *          bloquante).
 *
 * @warning Aucun secret (APN utilisateur/mot de passe SIM, identifiants
 *          Wi-Fi, identifiants MQTT) ne doit être stocké ici en clair (R-12).
 *          Ce fichier ne contient que des paramètres structurels et des
 *          valeurs par défaut non sensibles.
 */
#pragma once

#include <cstdint>

namespace fortico {
namespace network {

// ============================================================================
// 1. Point d'accès cellulaire (APN) — structure uniquement
// ============================================================================

/**
 * @brief Nom du point d'accès (APN) par défaut, utilisé si aucune valeur
 *        n'est provisionnée en NVS.
 * @warning [ASSUMPTION] Valeur d'exemple générique — à remplacer par l'APN
 *          réel de l'opérateur SIM utilisé en flotte, idéalement via
 *          provisioning NVS plutôt qu'en dur ici.
 */
constexpr const char* APN_DEFAULT = "internet";

// ============================================================================
// 2. FSM modem SIM7600E-L1C (cellulaire + GNSS intégré) — Document 05 §2.1
// ============================================================================

/// Timeout de la phase de handshake AT initial
constexpr uint32_t MODEM_AT_HANDSHAKE_TIMEOUT_MS = 5000;

/// Timeout d'attente de l'enregistrement réseau (NETWORK_REGISTERING)
constexpr uint32_t MODEM_NETWORK_REGISTRATION_TIMEOUT_MS = 30000;

/// Timeout d'établissement du contexte PDP
constexpr uint32_t MODEM_PDP_CONTEXT_TIMEOUT_MS = 15000;

// ============================================================================
// 3. Politique de reconnexion (back-off exponentiel) — SOP §3.1 (R-01)
// ============================================================================

/// Délai initial avant la première tentative de reconnexion
constexpr uint32_t RECONNECT_BACKOFF_MIN_MS = 2000;

/// Plafond du délai de reconnexion (back-off exponentiel borné)
constexpr uint32_t RECONNECT_BACKOFF_MAX_MS = 60000;

/// Facteur multiplicatif appliqué à chaque échec de reconnexion
constexpr uint8_t RECONNECT_BACKOFF_MULTIPLIER = 2;

/**
 * @brief Nombre d'échecs de reconnexion modem consécutifs avant power-cycle
 *        matériel du modem (si câblé) ou reboot ESP32 contrôlé.
 * @warning [ASSUMPTION] Seuil non fourni dans les documents sources.
 */
constexpr uint8_t MODEM_MAX_CONSECUTIVE_FAILURES_BEFORE_POWERCYCLE = 10;

// ============================================================================
// 4. Provisioning Wi-Fi local — SOP §4.1 (R-07)
// ============================================================================

/// Durée maximale d'ouverture du portail de provisioning avant fermeture
/// automatique de sécurité [ASSUMPTION]
constexpr uint32_t WIFI_PROVISIONING_PORTAL_TIMEOUT_MS = 300000;

// ============================================================================
// 5. Connexion au broker MQTT — Document 08 §2, §6
//    [DÉPLACÉ] Anciennement dans mqtt_config.h, supprimé de l'arborescence.
//    Fusionné ici avec la config réseau générique conformément à la liste de
//    configuration à 9 fichiers (pas de mqtt_config.h séparé).
// ============================================================================

/**
 * @brief Hôte du broker MQTT Fortico.
 * @warning [ASSUMPTION] Valeur d'exemple — à confirmer avec l'équipe backend
 *          avant implémentation figée (Document 08, note en tête).
 */
constexpr const char* MQTT_BROKER_HOST = "mqtt.fortico.example";

/// Port du broker MQTT — TLS obligatoire (R-05, SOP §4.1)
constexpr uint16_t MQTT_BROKER_PORT = 8883;

/// Intervalle keepalive MQTT (PINGREQ/PINGRESP)
constexpr uint16_t MQTT_KEEPALIVE_INTERVAL_S = 60;

/// Préfixe de l'identifiant client MQTT, complété par device_id à l'exécution
constexpr const char* MQTT_CLIENT_ID_PREFIX = "FORTICO-";

/// Taille maximale d'un payload JSON MQTT (garde-fou anti-dépassement)
constexpr uint32_t MQTT_MAX_PAYLOAD_SIZE_BYTES = 2048;

/// Nombre de tentatives de publication avant abandon et repli sur le
/// backlog SD (Document 02 §6)
constexpr uint8_t MQTT_PUBLISH_MAX_RETRIES = 3;

// ============================================================================
// 6. Authentification device/cloud (JWT) — Document 05 (AuthenticationService)
//    [NOUVEAU] Distinct de l'authentification RFID chauffeur (Document 06,
//    AuthenticationManager). Concerne l'identité du BOÎTIER auprès du cloud.
// ============================================================================
    
/// Durée de validité par défaut d'un token JWT device avant renouvellement
/// [ASSUMPTION — paramètre non fourni dans les documents sources]
constexpr uint32_t JWT_TOKEN_TTL_S = 3600;

/// Marge de sécurité avant expiration déclenchant un refreshToken()
/// [ASSUMPTION]
constexpr uint32_t JWT_REFRESH_MARGIN_S = 300;

}  // namespace network
}  // namespace fortico