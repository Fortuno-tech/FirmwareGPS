/**
 * @file    project_config.h
 * @brief   Version firmware, indicateurs de fonctionnalité (feature flags) et
 *          identité applicative du projet — Fortico.
 *
 * @details Périmètre strict (arborescence finale, 9 fichiers de
 *          configuration) : version, feature flags, préfixe d'identifiant
 *          device. L'identité de la carte physique (variante de puce,
 *          révision) n'a PAS de fichier dédié (board_config.h retiré de la
 *          liste) — si un besoin de distinction multi-carte apparaît, il
 *          sera réintégré ici sous forme de feature flag plutôt que dans un
 *          fichier séparé.
 *
 * @note    Source : Document 01 (Handbook), Sprint 0 §5.
 */
#pragma once

namespace fortico {
namespace project {

// ============================================================================
// 1. Identité et version
// ============================================================================

/// Version sémantique du firmware (MAJOR.MINOR.PATCH)
constexpr const char* FIRMWARE_VERSION = "1.0.0";

/// Préfixe de l'identifiant unique du boîtier, complété par le MAC ESP32 ou
/// un identifiant de provisioning à l'exécution
constexpr const char* DEVICE_ID_PREFIX = "FORTICO-";

/// Nom du projet, utilisé dans les logs et rapports de diagnostic
constexpr const char* PROJECT_NAME = "Fortico";

// ============================================================================
// 2. Indicateurs de fonctionnalité 
// ============================================================================

/// Active le FuelManager / fuelDriver (GPIO36, provisoire — voir
/// pin_config.h §9, Document 10 §5.3). Permet de désactiver proprement la
/// fonctionnalité carburant tant que le choix de broche/jauge n'est pas
/// validé par l'équipe hardware, sans supprimer le code.
constexpr bool FEATURE_FUEL_MANAGER_ENABLED = true;

/// Active la sonde de température (GPIO32)
constexpr bool FEATURE_TEMPERATURE_SENSOR_ENABLED = true;

/// Active l'authentification device/cloud par JWT (AuthenticationService) —
///  désactivable si le backend n'est pas encore prêt à
/// délivrer/valider des tokens, sans bloquer le reste de la connectivité MQTT
constexpr bool FEATURE_JWT_DEVICE_AUTH_ENABLED = false;

/// Active DiagnosticsManager/Service (runChecks périodiques)
constexpr bool FEATURE_DIAGNOSTICS_ENABLED = true;

/// Active PowerManager (gestion de modes d'alimentation) — [ASSUMPTION]
/// désactivé par défaut tant que les modes réels (SLEEP notamment) ne sont
/// pas spécifiés fonctionnellement
constexpr bool FEATURE_POWER_MANAGER_ENABLED = false;

/// Active OTA — permet un kill-switch d'urgence indépendant de R-13
constexpr bool FEATURE_OTA_ENABLED = true;

// ============================================================================
// 3. Build
// ============================================================================

/// [ASSUMPTION] Positionné par platformio.ini (build_flags), documenté ici
/// pour référence — la valeur réelle vient de CORE_DEBUG_LEVEL, pas de cette
/// constante
constexpr bool IS_PRODUCTION_BUILD =
#ifdef NDEBUG
    true;
#else
    false;
#endif

}  // namespace project
}  // namespace fortico