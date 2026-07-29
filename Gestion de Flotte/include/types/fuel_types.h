/**
 * @file    fuel_types.h
 * @brief   Niveau et consommation carburant — Fortico.
 *
 * @note    [ÉCART] Champ renommé theftDetected -> anomalyDetected : le
 *          Document 06 §5 (FuelManager) et §7 (table AlertManager) utilisent
 *          le code d'alerte FUEL_ANOMALY, plus général qu'un vol (peut aussi
 *          signaler une fuite ou un capteur défaillant) — cohérence de
 *          nommage avec ces documents déjà livrés.
 * @warning FuelDriver/FuelManager reposent sur une broche provisoire
 *          (GPIO36, non validée hardware) — voir Document 10 §5.3.
 */
#pragma once

namespace fortico {
namespace types {

struct FuelLevel {
    float liters;
    float percentage;
    float consumptionRateLph;  ///< litres/heure estimés, FuelManager::getConsumption() (Document 06 §5)
    bool anomalyDetected;       ///< déclenche EventID::FUEL_ANOMALY, jamais une coupure relais (Document 06 §2.3)
};

}  // namespace types
}  // namespace fortico
