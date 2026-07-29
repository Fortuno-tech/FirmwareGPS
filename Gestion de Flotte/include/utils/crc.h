/**
 * @file    crc.h
 * @brief   CRC32 générique — Fortico.
 *
 * @details Utilisé notamment pour la validation d'intégrité des fichiers de
 *          calibration (assets/calibration/*.json, Document 04 §6) avant
 *          chargement, et pour tout futur besoin de validation de trame.
 */
#pragma once

#include <cstdint>
#include <cstddef>

namespace fortico {
namespace utils {

class CRC32 {
public:
    static uint32_t calculate(const uint8_t* data, size_t length);
};

}  // namespace utils
}  // namespace fortico
