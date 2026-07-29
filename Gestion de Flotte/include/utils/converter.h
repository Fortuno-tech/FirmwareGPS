/**
 * @file    converter.h
 * @brief   Conversions ADC -> grandeurs physiques — Fortico.
 *
 * @note    [AMÉLIORATION] Les implémentations (.cpp, Sprint 1/2) doivent
 *          utiliser les constantes déjà définies dans hardware_config.h
 *          (BATTERY_VOLTAGE_DIVIDER_RATIO, FUEL_SENDER_RATIO_PLACEHOLDER,
 *          ADC_ATTENUATION_DB) plutôt que des valeurs magiques recopiées —
 *          absent de la proposition initiale, qui ne précisait pas la
 *          source des coefficients.
 */
#pragma once

#include <cstdint>

namespace fortico {
namespace utils {

class Converter {
public:
    /// Tension brute (mV) depuis une lecture ADC calibrée (AdcHal::readMv())
    static float adcToVoltage(uint16_t rawOrMv);

    /// Tension réelle batterie (après application de hw::BATTERY_VOLTAGE_DIVIDER_RATIO)
    static float voltageToBattery(float measuredVoltage);

    /// Niveau carburant (%) depuis une lecture ADC, via la courbe de
    /// calibration assets/calibration/fuel_calibration.json (Document 04 §6)
    /// — fallback sur hw::FUEL_SENDER_RATIO_PLACEHOLDER si le fichier est absent/invalide
    static float adcToFuel(uint16_t raw);
};

}  // namespace utils
}  // namespace fortico
