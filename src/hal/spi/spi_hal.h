/**
 * @file    spi_hal.h
 * @brief   Abstraction HAL du bus SPI (ESP32/VSPI) — Fortico.
 *
 * @details Expose :
 *            - init() / transfer() / write() / read()
 *            - native() : accès à l'objet SPIClass sous-jacent,
 *              nécessaire pour les bibliothèques tierces (SD.h)
 *              qui acceptent un SPIClass& en paramètre.
 *
 * @note    native() n'est disponible que sous ESP32_TARGET.
 *          Tout appel depuis un Driver doit être conditionné par
 *          #ifdef ESP32_TARGET pour rester portable.
 */
#pragma once
#include "config/hardware_config.h"
#include <cstddef>
#include <cstdint>

#ifdef ESP32_TARGET
#include <SPI.h>
#endif

namespace fortico {
namespace hal {

class SpiHal {
public:
    bool init(int clkPin, int mosiPin, int misoPin, int csPin);

    bool transfer(const uint8_t* tx, uint8_t* rx, size_t length);
    bool write(const uint8_t* tx, size_t length);
    bool read(uint8_t* rx, size_t length);

#ifdef ESP32_TARGET
    /**
     * @brief   Accès à l'objet SPIClass natif.
     *
     * @details Utilisé exclusivement par SdDriver pour passer
     *          le bon bus SPI à SD.begin().
     *          Ne pas appeler depuis les couches Manager/Service.
     *
     * @pre     init() doit avoir été appelé avec succès.
     * @return  Référence vers l'objet SPIClass interne.
     */
    SPIClass& native();
#endif

private:
#ifdef ESP32_TARGET
    SPIClass spi_;
#endif
    bool initialized_ = false;
};

}  // namespace hal
}  // namespace fortico
