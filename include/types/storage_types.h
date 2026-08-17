/**
 * @file    storage_types.h
 * @brief   Enregistrement de journalisation locale (boîte noire SD) —
 *          Fortico.
 *
 * @note    Reflète le format JSON Lines déjà spécifié au Document 09 §7.
 *          Ce struct est la représentation EN MÉMOIRE avant sérialisation ;
 *          StorageService (Document 05 §6) l'encode en JSON Lines à
 *          l'écriture.
 */
#pragma once

#include <cstdint>

namespace fortico {
namespace types {

enum class LogRecordType {
    GPS,
    AUTH,
    RELAY,
    ALERT,
    SENSOR,
};

/**
 * @brief Enregistrement générique. Selon `type`, seul le sous-ensemble de
 *        champs pertinent est renseigné (les autres restent à leur valeur
 *        par défaut) — approche simple adaptée à l'embarqué, à défaut d'un
 *        variant/union C++17 std::variant si la taille mémoire le permet.
 */
struct LogRecord {
    uint32_t seq;
    uint64_t timestamp;
    LogRecordType type;

    // GPS
    double latitude;
    double longitude;
    float speedKmh;

    // AUTH
    char rfidUid[16];
    bool authResult;

    // RELAY
    uint8_t relayState;   ///< types::RelayState encodé en uint8_t pour compacité de stockage
    uint8_t relaySource;  ///< types::RelaySource encodé en uint8_t

    // ALERT
    uint8_t alertCode;     ///< types::AlertCode encodé en uint8_t
    uint8_t alertSeverity; ///< types::AlertSeverity encodé en uint8_t

    // SENSOR
    float batteryVoltage;
    float temperatureC;
    float fuelPercentage;
};

}  // namespace types
}  // namespace fortico
