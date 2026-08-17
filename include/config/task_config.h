/**
 * @file    task_config.h
 * @brief   Priorités, tailles de stack et affectation de cœur des tâches
 *          FreeRTOS (Fortico).
 *
 * @details Périmètre strict (Sprint 0, §5) : uniquement les paramètres de
 *          création des tâches. Aucune logique d'ordonnancement ici — voir
 *          Scheduler (core/) pour la logique, ce fichier ne fournit que les
 *          constantes consommées à la création de chaque tâche.
 *
 * @note    Source : Document 07 (RTOS Design Guide, §1, §2).
 * @warning Toute modification de priorité doit être répercutée dans le
 *          Document 07 avant merge — en particulier la règle : aucune tâche
 *          réseau (cœur 0) ne doit dépasser la priorité des tâches de
 *          sécurité (RelayManagerTask, SystemControllerTask).
 *
 * @note    [MISE À JOUR — architecture UML finale] Tous les nouveaux
 *          composants de l'UML « finale » n'obtiennent pas systématiquement
 *          une tâche FreeRTOS dédiée. Règle retenue : un composant obtient sa
 *          propre tâche s'il fait de l'E/S bloquante potentielle (réseau,
 *          bus, fichier) ou porte une contrainte temps-réel/sécurité propre ;
 *          sinon il est invoqué en synchrone dans la tâche du composant
 *          appelant. Répartition retenue :
 *            - TaskManager, AppStateMachine (L5) : objets internes à
 *              SystemControllerTask, pas de tâche séparée.
 *            - FuelManager (L4) : logique invoquée en synchrone dans
 *              SensorSamplingTask après lecture ADC carburant, pas de tâche
 *              séparée.
 *            - TelemetryManager (L4) et TelemetryService (L3) : partagent la
 *              même tâche (TelemetryServiceTask) — le Manager bufferise et
 *              décide, le Service encode ; les deux s'exécutent dans le même
 *              contexte pour éviter un aller-retour de queue inutile.
 *            - AuthenticationService/JWT et SecurityService (L3) : appels
 *              synchrones depuis MQTTServiceTask/StorageServiceTask, pas de
 *              tâche séparée (opérations courtes, non bloquantes sur E/S).
 *            - DiagnosticsManager/Service et PowerManager (L4/L3) : nouvelles
 *              tâches dédiées basse priorité, périodiques — voir ci-dessous.
 */
#pragma once

#include <cstdint>
#include <freertos/FreeRTOS.h>

namespace fortico {
namespace tasks {

// ============================================================================
// Affectation des cœurs — Document 07 §1
// ============================================================================

/// Cœur hébergeant la logique métier temps-réel et le contrôle du relais
constexpr BaseType_t CORE_APP_CRITICAL = 1;   // APP_CPU

/// Cœur hébergeant la connectivité et la télémétrie
constexpr BaseType_t CORE_NETWORK = 0;        // PRO_CPU

// ============================================================================
// Core 1 (APP_CPU) — tâches métier et sécurité — Document 07 §2
// ============================================================================

/// RelayManagerTask — seule tâche autorisée à écrire GPIO12
constexpr UBaseType_t RELAY_MANAGER_TASK_PRIORITY = 6;
constexpr uint32_t    RELAY_MANAGER_TASK_STACK_WORDS = 2048;

/// EventBusTask — [NOUVEAU, 12/07/2026] dispatch centralisé IPublisher/
/// ISubscriber pour les événements non critiques (alertes, télémétrie,
/// stockage, diagnostics, auth). NE PORTE JAMAIS q_relayCmd — écriture
/// directe conservée pour ce chemin (Document 07 §4bis.1).
constexpr UBaseType_t EVENT_BUS_TASK_PRIORITY = 5;
constexpr uint32_t    EVENT_BUS_TASK_STACK_WORDS = 3072;

/// SystemControllerTask — FSM globale, arbitrage inter-managers
constexpr UBaseType_t SYSTEM_CONTROLLER_TASK_PRIORITY = 5;
constexpr uint32_t    SYSTEM_CONTROLLER_TASK_STACK_WORDS = 4096;

/// AuthenticationManagerTask — lecture PN532, décision d'accès
constexpr UBaseType_t AUTH_MANAGER_TASK_PRIORITY = 4;
constexpr uint32_t    AUTH_MANAGER_TASK_STACK_WORDS = 3072;

/// TrackingManagerTask — parsing NMEA, calcul position/vitesse
constexpr UBaseType_t TRACKING_MANAGER_TASK_PRIORITY = 4;
constexpr uint32_t    TRACKING_MANAGER_TASK_STACK_WORDS = 4096;

/// AlertManagerTask — buzzer/LED, motifs d'alerte
constexpr UBaseType_t ALERT_MANAGER_TASK_PRIORITY = 3;
constexpr uint32_t    ALERT_MANAGER_TASK_STACK_WORDS = 2048;

/// StorageManagerTask — écriture SD boîte noire
constexpr UBaseType_t STORAGE_MANAGER_TASK_PRIORITY = 3;
constexpr uint32_t    STORAGE_MANAGER_TASK_STACK_WORDS = 4096;

/// SensorSamplingTask — ADC batterie/température/carburant + filtrage
/// (inclut désormais l'évaluation FuelManager en synchrone après lecture)
constexpr UBaseType_t SENSOR_SAMPLING_TASK_PRIORITY = 2;
constexpr uint32_t    SENSOR_SAMPLING_TASK_STACK_WORDS = 2048;

/// DiagnosticsManagerTask — runChecks() périodique (heap, stack,
/// santé bus), agrège vers DiagnosticsService
constexpr UBaseType_t DIAGNOSTICS_MANAGER_TASK_PRIORITY = 3;  // [CORRIGÉ 12/07/2026, était 1 — aligné sur Document 07 v3.0
constexpr uint32_t    DIAGNOSTICS_MANAGER_TASK_STACK_WORDS = 3072;
constexpr uint32_t    DIAGNOSTICS_MANAGER_TASK_PERIOD_MS = 10000;

/// PowerManagerTask — [NOUVEAU] gestion des modes d'alimentation, réactif aux
/// transitions d'AppStateMachine (ex. SLEEP) [ASSUMPTION — modes non détaillés
/// dans les documents sources]
constexpr UBaseType_t POWER_MANAGER_TASK_PRIORITY = 2;
constexpr uint32_t    POWER_MANAGER_TASK_STACK_WORDS = 2048;

// ============================================================================
// Core 0 (PRO_CPU) — tâches réseau et connectivité — Document 07 §2
// ============================================================================

/// NetworkServiceTask — FSM modem SIM7600, enregistrement réseau
constexpr UBaseType_t NETWORK_SERVICE_TASK_PRIORITY = 4;
constexpr uint32_t    NETWORK_SERVICE_TASK_STACK_WORDS = 6144;

/// MQTTServiceTask — publish/subscribe, handshake TLS
constexpr UBaseType_t MQTT_SERVICE_TASK_PRIORITY = 4;
constexpr uint32_t    MQTT_SERVICE_TASK_STACK_WORDS = 8192;

/// TelemetryServiceTask — agrégation JSON, throttling ; héberge aussi la
/// logique de TelemetryManager (bufferisation/décision de publication), les
/// deux composants partageant ce contexte d'exécution
constexpr UBaseType_t TELEMETRY_SERVICE_TASK_PRIORITY = 3;
constexpr uint32_t    TELEMETRY_SERVICE_TASK_STACK_WORDS = 4096;

/// OTAServiceTask — téléchargement/flash signé
constexpr UBaseType_t OTA_SERVICE_TASK_PRIORITY = 3;  // [CORRIGÉ 12/07/2026, était 2 — aligné sur Document 07 v3.0
constexpr uint32_t    OTA_SERVICE_TASK_STACK_WORDS = 8192;

/// ConfigServiceTask — [NOUVEAU] paramètres runtime modifiables à chaud
/// (persistés NVS), distinct des fichiers config/ statiques compilés en dur
constexpr UBaseType_t CONFIG_SERVICE_TASK_PRIORITY = 2;
constexpr uint32_t    CONFIG_SERVICE_TASK_STACK_WORDS = 2048;

/// SecurityServiceTask — [NOUVEAU, si FEATURE activée] encrypt/decrypt/hash applicatifs
constexpr UBaseType_t SECURITY_SERVICE_TASK_PRIORITY = 2;
constexpr uint32_t    SECURITY_SERVICE_TASK_STACK_WORDS = 4096;

/// TimeServiceTask — [NOUVEAU] synchronisation NTP, jamais sur le chemin
/// critique du démarrage (Document 07 §4bis.2)
constexpr UBaseType_t TIME_SERVICE_TASK_PRIORITY = 1;
constexpr uint32_t    TIME_SERVICE_TASK_STACK_WORDS = 2048;

/// LoggerTask — sérialisation logs, rotation
constexpr UBaseType_t LOGGER_TASK_PRIORITY = 1;
constexpr uint32_t    LOGGER_TASK_STACK_WORDS = 2048;

/// SchedulerTask — timers logiciels, flush NVS périodique
constexpr UBaseType_t SCHEDULER_TASK_PRIORITY = 1;
constexpr uint32_t    SCHEDULER_TASK_STACK_WORDS = 2048;

// ============================================================================
// Files (queues) — tailles de référence — Document 07 §3
// ============================================================================

constexpr uint8_t QUEUE_LEN_GPS_FIX        = 5;
constexpr uint8_t QUEUE_LEN_RFID_EVENT     = 3;
constexpr uint8_t QUEUE_LEN_RELAY_CMD      = 5;
constexpr uint8_t QUEUE_LEN_ALERT          = 10;
constexpr uint8_t QUEUE_LEN_STORAGE        = 20;
constexpr uint8_t QUEUE_LEN_TELEMETRY_OUT  = 10;
constexpr uint8_t QUEUE_LEN_MQTT_TX        = 10;
constexpr uint8_t QUEUE_LEN_MQTT_RX        = 5;
constexpr uint8_t QUEUE_LEN_NETWORK_EVENT  = 5;

// ============================================================================
// EventBus — Document 07 §4bis 
// ============================================================================

/// File d'entrée d'EventBusTask — tout publish() y écrit avant dispatch
constexpr uint8_t QUEUE_LEN_EVENT_BUS_IN = 20;

/// Taille par défaut de chaque file interne de manager abonné (qAuth, qAlert,
/// qStorage, qTelemetry, qDiag), alimentée par le onEvent() de chaque
/// IEventListener — jamais par écriture directe d'un autre composant
constexpr uint8_t QUEUE_LEN_EVENT_BUS_SUBSCRIBER_DEFAULT = 10;

///  N'est plus utilisée par EventBus depuis la révision
/// qui indexe directement le tableau d'abonnements par constants::EventID
/// (taille = EventID::EVENT_ID_COUNT, voir constants/events.h et
/// core/event_bus/event_bus.h) — plus besoin d'une taille de table de topics
/// séparée. Conservée ici (non supprimée) pour ne pas casser une référence
/// externe éventuelle
constexpr uint8_t EVENT_BUS_MAX_TOPICS = 16;

/// Nombre maximum d'abonnés (IEventListener*) par topic
constexpr uint8_t EVENT_BUS_MAX_LISTENERS_PER_TOPIC = 4;

}  // namespace tasks
}  // namespace fortico