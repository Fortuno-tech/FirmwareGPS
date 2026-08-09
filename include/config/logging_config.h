/**
 * @file    logging_config.h
 * @brief   Niveaux de log, tags par module et politique de rotation des logs
 *          SD (Fortico).
 *
 * @details Périmètre strict (Sprint 0, §5) : uniquement la configuration du
 *          Logger et de la journalisation locale — jamais le contenu d'un
 *          log lui-même. Le format des enregistrements de la boîte noire est
 *          spécifié au Document 09 §7 ; ce fichier ne fixe que sa politique
 *          de rotation/flush.
 *
 * @note    Source : Document 06 (StorageManager), Document 09 §7, SOP §5
 *          (R-14 — logs de débogage désactivés en production).
 * @warning CORE_DEBUG_LEVEL est un flag de compilation PlatformIO
 *          (build_flags = -DCORE_DEBUG_LEVEL=0 en production), pas une
 *          constante de ce fichier — voir platformio.ini.
 */
#pragma once

#include <cstdint>

namespace fortico {
namespace logging {

// ============================================================================
// 1. Niveaux de log applicatifs (indépendants de CORE_DEBUG_LEVEL)
// ============================================================================

enum class Level : uint8_t {
    LOG_ERROR = 0,
    LOG_WARN  = 1,
    LOG_INFO  = 2,
    LOG_DEBUG = 3,
};

/// Niveau de log minimal émis vers la console série en production
constexpr Level PRODUCTION_MIN_LEVEL = Level::LOG_WARN;

/// Niveau de log minimal émis vers la console série en développement
constexpr Level DEVELOPMENT_MIN_LEVEL = Level::LOG_DEBUG;

// ============================================================================
// 2. Tags par module — cohérence avec la table des tâches (task_config.h)
// ============================================================================

constexpr const char* TAG_SYSTEM_CONTROLLER = "SYSCTRL";
constexpr const char* TAG_RELAY             = "RELAY";
constexpr const char* TAG_AUTH              = "AUTH";
constexpr const char* TAG_TRACKING          = "TRACKING";
constexpr const char* TAG_FUEL              = "FUEL";
constexpr const char* TAG_ALERT             = "ALERT";
constexpr const char* TAG_STORAGE           = "STORAGE";
constexpr const char* TAG_NETWORK           = "NETWORK";
constexpr const char* TAG_MQTT              = "MQTT";
constexpr const char* TAG_TELEMETRY         = "TELEMETRY";
constexpr const char* TAG_OTA               = "OTA";
constexpr const char* TAG_SECURITY          = "SECURITY";
constexpr const char* TAG_DIAGNOSTICS       = "DIAG";       
constexpr const char* TAG_POWER             = "POWER";      
constexpr const char* TAG_APP_STATE         = "APPSTATE";  
constexpr const char* TAG_TASK_MANAGER      = "TASKMGR";    
constexpr const char* TAG_JWT_AUTH          = "JWTAUTH";  


// ============================================================================
// 3. Politique de journalisation locale (boîte noire SD) — Document 09 §7
// ============================================================================

/// Répertoire racine des journaux sur la carte SD
constexpr const char* SD_LOG_DIRECTORY = "/logs";

/// Gabarit de nom de fichier journalier (rotation à 00:00 RTC)
constexpr const char* SD_LOG_FILENAME_FORMAT = "%04d%02d%02d.jsonl";  // YYYYMMDD.jsonl

/// Fichier curseur de synchronisation cloud (Document 09 §7)
constexpr const char* SD_LOG_CURSOR_FILE = "/logs/.cursor";

/// Sévérité à partir de laquelle un enregistrement force un flush immédiat
constexpr Level SD_LOG_IMMEDIATE_FLUSH_MIN_SEVERITY = Level::LOG_WARN;
constexpr std::size_t LOG_LINE_MAX_LENGTH = 128;

constexpr uint8_t QUEUE_LEN_LOGGER = 32;

}  // namespace logging
}  // namespace fortico