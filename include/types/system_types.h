/**
 * @file    system_types.h
 * @brief   Types système transverses : cycle de vie global, santé système,
 *          événement EventBus — Fortico.
 */
#pragma once

#include <cstdint>
#include "constants/events.h"

namespace fortico {
namespace types {

/// Cycle de vie système global, porté par AppStateMachine (Document 02 §5.1)
enum class SystemState {
    BOOT,
    INIT,
    NETWORK,
    OPERATIONAL,
    SLEEP,     ///< [ASSUMPTION, PowerManager désactivé par défaut] non utilisé tant que FEATURE_POWER_MANAGER_ENABLED=false
    UPDATE,
    ERROR,
};

/**
 * @brief Événement léger transitant sur l'EventBus (IPublisher/ISubscriber).
 *
 * @note  Volontairement minimal : l'EventBus sert à NOTIFIER qu'un
 *        événement a eu lieu (ex. "un badge a été refusé"), pas à
 *        transporter la donnée métier complète. La donnée détaillée continue
 *        de transiter par les files FreeRTOS dédiées déjà spécifiées au
 *        Document 07 §3 (q_alert, q_relayCmd, q_gpsFix…). Un ISubscriber qui
 *        a besoin du détail va le chercher via l'API du composant émetteur,
 *        pas via ce struct.
 */
struct Event {
    constants::EventID id;
    uint64_t timestamp;
    uint32_t auxData;   ///< usage libre, ex. code d'erreur ou identifiant secondaire — pas de payload riche ici
};

/// Agrégat de santé système, DiagnosticsManager::getHealth() (Document 06 §10)
struct SystemHealth {
    uint32_t freeHeapBytes;
    uint32_t minFreeHeapBytes;
    uint16_t minStackWordsRelayTask;   ///< uxTaskGetStackHighWaterMark(RelayManagerTask)
    uint8_t i2cConsecutiveFailures;
    uint8_t sim7600ConsecutiveFailures;
    bool heapWithinBudget;              ///< faux si < 20% libre (R-10, SOP §5)
};

/// Rapport agrégé, DiagnosticsService::getReport() (Document 06 §10)
struct DiagnosticReport {
    SystemHealth health;
    uint64_t reportTimestamp;
    uint32_t uptimeSeconds;
};

}  // namespace types
}  // namespace fortico
