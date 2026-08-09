/**
 * @file    gps_types.h
 * @brief   Structure de position GNSS — Fortico.
 *
 * @note    [ÉCART] Nommée GpsData (pas GPSFix comme suggéré) pour rester
 *          cohérent avec les Documents 04 §5.2 et 07 §3, déjà livrés et
 *          référençant explicitement "GpsData (include/types/gps_types.h)".
 *          Casse alignée sur la convention Sim7600Driver (pas SIM7600Driver)
 *          — Document 03 §2.
 */
#pragma once

#include <cstdint>

namespace fortico {
namespace types {

struct GpsData {
    double latitude;
    double longitude;
    float speedKmh;     ///< si !fixValid, DOIT valoir constants::INVALID_SPEED (constants/default_values.h) — jamais 0.0f, voir note de sûreté dans ce fichier
    float headingDeg;
    uint8_t satellites;
    bool fixValid;      ///< Document 06 §4 : nécessite satellites >= GPS_MIN_SATELLITES_FOR_FIX
    uint64_t timestamp; ///< horodatage RTC (RtcDriver) ou fallback monotone (RtcHal) si RTC indisponible
};

}  // namespace types
}  // namespace fortico