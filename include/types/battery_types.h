/**
 * @file    battery_types.h
 * @brief   État de la batterie véhicule — Fortico.
 */
#pragma once

namespace fortico {
namespace types {

struct BatteryState {
    float voltage;       ///< tension réelle après pont diviseur (hardware_config.h)
    float percentage;    ///< estimation grossière, non linéaire par nature (plomb-acide 12V)
    bool isCritical;      ///< vrai si voltage < hw::BATTERY_CRITICAL_VOLTAGE (hardware_config.h)
};

}  // namespace types
}  // namespace fortico
