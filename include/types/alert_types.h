/**
 * @file    alert_types.h
 * @brief   Types d'alerte consommés par AlertManager — Fortico.
 *
 * @note    AlertCode et la sévérité par défaut reprennent exactement la
 *          table du Document 06 §7.
 */
#pragma once

#include <cstdint>

namespace fortico {
namespace types {

enum class AlertCode {
    AUTH_DENIED,
    SOS_BUTTON,
    BATTERY_LOW,
    FUEL_ANOMALY,
    GPS_LOST,
    RELAY_FAULT,
    NETWORK_LOST,
    DIAGNOSTIC_WARNING,
};

enum class AlertSeverity : uint8_t {
    LOW,
    MEDIUM,
    HIGH,
};

struct AlertEvent {
    AlertCode code;
    AlertSeverity severity;
    uint64_t timestamp;
};

}  // namespace types
}  // namespace fortico
