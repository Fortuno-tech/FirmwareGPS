/**
 * @file    adc_calibration.h
 * @brief   Fichier de calibration ADC (Fortico).
 *
 * @details Contient les ratios et offsets calibrés physiquement sur banc d'essai
 *          pour la mesure de tension batterie, sonde de température et jauge carburant.
 *          Généré ou mis à jour via tools/calibrate_adc.py.
 */
#pragma once

namespace fortico {
namespace calibration {

/// Ratio du pont diviseur de tension batterie (12V nominal -> ADC 0-3.3V)
/// R1 = 30k, R2 = 10k -> ratio théorique = (30+10)/10 = 4.0
constexpr float BATTERY_DIVIDER_RATIO = 4.0F;

/// Calibration de l'offset batterie en Volts (correction diode de protection/chute)
constexpr float BATTERY_OFFSET_VOLTS = 0.0F;

/// Ratio de conversion jauge carburant (Volts vers %)
constexpr float FUEL_SENDER_DIVIDER_RATIO = 1.0F;

/// Offset jauge carburant en Volts (0% réservoir)
constexpr float FUEL_MIN_VOLTAGE = 0.5F;

/// Tension jauge carburant réservoir plein (100%)
constexpr float FUEL_MAX_VOLTAGE = 3.0F;

}  // namespace calibration
}  // namespace fortico

