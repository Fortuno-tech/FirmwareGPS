/**
 * @file    gpio_map.h
 * @brief   Table de correspondance GPIO -> nom logique, pour les logs et le
 *          diagnostic — Fortico.
 *
 * @details Distinct de config/pin_config.h : pin_config.h DÉFINIT les
 *          constantes GPIO (source de vérité matérielle) ; gpio_map.h
 *          fournit uniquement une fonction de résolution GPIO -> chaîne
 *          lisible, utile pour DiagnosticsService::getReport() (Document 06
 *          §10) et pour tout message de log mentionnant une broche
 *          (traçabilité en boîte noire, Document 09 §7).
 *          Ce fichier ne doit jamais être utilisé pour piloter un GPIO —
 *          seul pin_config.h fait foi pour l'affectation matérielle.
 */
#pragma once

#include <cstdint>
#include "config/pin_config.h"

namespace fortico {
namespace constants {

/**
 * @brief Résout un numéro de GPIO en nom logique, pour affichage dans les
 *        logs/diagnostics. Retourne "UNKNOWN_GPIO" si non répertorié.
 */
inline const char* gpioToName(uint8_t gpio) {
    switch (gpio) {
        case fortico::pins::I2C_SDA:        return "I2C_SDA";
        case fortico::pins::I2C_SCL:        return "I2C_SCL";
        case fortico::pins::SD_CS:          return "SD_CS";
        case fortico::pins::SD_SCK:         return "SD_SCK";
        case fortico::pins::SD_MOSI:        return "SD_MOSI";
        case fortico::pins::SD_MISO:        return "SD_MISO";
        case fortico::pins::SIM7600_TX1:    return "SIM7600_TX1";
        case fortico::pins::SIM7600_RX1:    return "SIM7600_RX1";
        case fortico::pins::RELAY_CTRL:     return "RELAY_CTRL";
        case fortico::pins::BUZZER_SIGNAL:  return "BUZZER_SIGNAL";
        case fortico::pins::LED_INDICATOR:  return "LED_INDICATOR";
        case fortico::pins::BUTTON_SOS:     return "BUTTON_SOS";
        case fortico::pins::BATTERY_ADC:    return "BATTERY_ADC";
        case fortico::pins::TEMPERATURE_ADC:return "TEMPERATURE_ADC";
        case fortico::pins::FUEL_ADC:       return "FUEL_ADC";
        default:                            return "UNKNOWN_GPIO";
    }
}

}  // namespace constants
}  // namespace fortico
