/**
 * @file    telemetry_types.h
 * @brief   Paquet de télémétrie agrégé — Fortico.
 *
 * @note    Champ fuel AJOUTÉ par rapport à la proposition initiale : le
 *          Document 09 (API & JSON Specification) a été écrit avant la
 *          formalisation de FuelManager et ne liste pas encore de champ
 *          "fuel" dans son exemple de payload JSON (§2). Ce struct est
 *          correct et complet dès maintenant ; le Document 09 devra être mis
 *          à jour séparément pour refléter ce champ dans son schéma JSON
 *          (ajouté au registre des écarts, Document 00 §4).
 */
#pragma once

#include <cstdint>
#include "types/gps_types.h"
#include "types/battery_types.h"
#include "types/fuel_types.h"
#include "types/relay_types.h"
#include "types/auth_types.h"

namespace fortico {
namespace types {

struct TelemetryPacket {
    uint32_t seq;              ///< Document 09 §2 — compteur monotone anti-trou
    uint64_t timestamp;

    GpsData gps;
    BatteryState battery;
    FuelLevel fuel;            
    RelayState relay;
    AuthenticationResult lastAuthResult;

    float temperatureC;
};

}  // namespace types
}  // namespace fortico
