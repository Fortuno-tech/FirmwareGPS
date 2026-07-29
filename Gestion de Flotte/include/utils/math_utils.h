/**
 * @file    math_utils.h
 * @brief   Utilitaires mathématiques génériques — Fortico.
 *
 * @note    lowPass() implémente le filtre EMA déjà spécifié au format
 *          hardware_config.h::ADC_EMA_ALPHA (Document 02 §6) : appelé avec
 *          alpha=ADC_EMA_ALPHA depuis AdcHal::readAvg() ou les drivers ADC.
 */
#pragma once

namespace fortico {
namespace utils {

class MathUtils {
public:
    static float clamp(float value, float min, float max);

    static float map(float x, float inMin, float inMax, float outMin, float outMax);

    /// Filtre passe-bas exponentiel : previous + alpha * (current - previous)
    static float lowPass(float current, float previous, float alpha);
};

}  // namespace utils
}  // namespace fortico
