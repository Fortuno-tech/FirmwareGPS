/**
 * @file    events.h
 * @brief   Identifiants d'événements métier pour le pattern EventBus
 *          (IPublisher/ISubscriber/IEventListener) — Fortico.
 *
 * @details Ne pas confondre avec les bits de systemEventGroup (Document 07,
 *          §4) : EventID/EventBus est un mécanisme logiciel de publication/
 *          souscription générique entre managers (ex. FuelManager publie
 *          FUEL_ANOMALY, AlertManager y souscrit) ; systemEventGroup est une
 *          primitive FreeRTOS native de bas niveau utilisée spécifiquement
 *          pour la synchronisation des transitions d'AppStateMachine.
 *          Les deux mécanismes coexistent pour des usages différents.
 *
 * @note    Consolidé à partir des codes d'alerte du Document 06 §7 et des
 *          événements de connectivité du Document 07 §4.
 *
 * @warning [AJOUT 13/07/2026] EVENT_ID_COUNT DOIT rester le DERNIER membre
 *          de l'enum. EventBus (core/event_bus) l'utilise pour dimensionner
 *          un tableau statique indexé directement par EventID (accès O(1),
 *          plus de recherche par chaîne — voir event_bus.h). Toute nouvelle
 *          valeur d'EventID s'ajoute AVANT EVENT_ID_COUNT, jamais après.
 */
#pragma once

#include <cstdint>

namespace fortico {
namespace constants {

enum class EventID : uint32_t {
    // Authentification (Document 06 §3, §7)
    RFID_CARD_READ,
    AUTH_AUTHORIZED,
    AUTH_DENIED,

    // Position (Document 04 §5.2, 06 §4)
    GPS_FIX_ACQUIRED,
    GPS_LOST,

    // Connectivité (Document 05, 07 §4)
    NETWORK_CONNECTED,
    NETWORK_LOST,
    MQTT_CONNECTED,
    MQTT_DISCONNECTED,

    // Relais / sûreté (Document 02 §5.2, 06 §2)
    RELAY_ENGAGED,
    RELAY_CUT,
    RELAY_EMERGENCY_CUT,
    RELAY_FAULT,

    // Alertes (Document 06 §7)
    SOS_BUTTON_PRESSED,
    FUEL_ANOMALY,
    BATTERY_LOW,
    DIAGNOSTIC_WARNING,

    // OTA (Document 05 §7)
    OTA_AVAILABLE,
    OTA_READY,
    OTA_STARTED,
    OTA_FAILED,
    OTA_SUCCEEDED,

    // Cycle de vie système 
    SYSTEM_SELFTEST_PASSED,
    SYSTEM_FAULT,

    //  Sentinelle — DOIT rester en dernier (voir avertissement ci-dessus)
    EVENT_ID_COUNT,
};

/**
 * @brief Résultat de IPublisher::publish() — [NOUVEAU 13/07/2026]
 *
 * @note  Remplace le simple bool d'origine : un échec de publish() peut
 *        avoir plusieurs causes distinctes (file pleine vs bus non
 *        initialisé), utiles à distinguer en diagnostic/test plutôt que de
 *        tout réduire à "faux".
 */
enum class PublishResult : uint8_t {
    OK,
    QUEUE_FULL,
    NOT_INITIALIZED,
};

}  // namespace constants
}  // namespace fortico