/**
 * @file    default_values.h
 * @brief   Valeurs sentinelles par défaut/invalides — Fortico.
 *
 * @details Ce ne sont pas des paramètres de configuration (hardware_config.h,
 *          project_config.h) : ce sont des valeurs de repli utilisées quand
 *          une lecture capteur échoue ou qu'une donnée n'a pas encore été
 *          acquise, pour qu'un champ numérique brut (float/uint32_t) reste
 *          auto-descriptif sans avoir besoin d'un booléen de validité
 *          parallèle pour chaque champ.
 *
 * @warning [POINT DE SÛRETÉ] INVALID_SPEED est directement lié à la règle de
 *          sûreté du relais (Document 02 §5.2, Document 06 §2.3) :
 *          RelayManager n'autorise une coupure non-urgente que si le
 *          véhicule est « à l'arrêt » (vitesse proche de zéro). Si
 *          GpsData::fixValid est faux, GpsData::speedKmh DOIT être positionné
 *          à INVALID_SPEED (jamais à 0.0f) — sinon une absence de fix GPS
 *          serait interprétée à tort comme "véhicule à l'arrêt" et
 *          autoriserait une coupure alors que le véhicule est peut-être en
 *          mouvement. Tout code lisant speedKmh doit vérifier fixValid
 *          d'abord ; INVALID_SPEED est un garde-fou défensif en second
 *          rideau, pas un remplacement de cette vérification.
 */
#pragma once

#include <cstdint>

namespace fortico {
namespace constants {

constexpr float INVALID_TEMPERATURE = -999.0f;   ///< TelemetryPacket::temperatureC si TemperatureDriver::isHealthy()==false
constexpr float INVALID_VOLTAGE = -1.0f;          ///< BatteryState::voltage si BatteryDriver::isHealthy()==false — distinct de isCritical (lecture valide mais basse)
constexpr float INVALID_SPEED = -1.0f;            ///< GpsData::speedKmh si fixValid==false — voir avertissement de sûreté ci-dessus
constexpr float INVALID_FUEL_PERCENTAGE = -1.0f;  ///< FuelLevel::percentage si FuelDriver::isHealthy()==false
constexpr uint32_t INVALID_TIMESTAMP = 0;          ///< horodatage non encore acquis (avant premier RTC_SYNCED, Document 07 §4)
constexpr uint8_t INVALID_SATELLITE_COUNT = 0;     ///< GpsData::satellites avant premier fix

}  // namespace constants
}  // namespace fortico