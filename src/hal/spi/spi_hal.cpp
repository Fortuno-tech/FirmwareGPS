/**
 * @file    spi_hal.cpp
 * @brief   Implémentation HAL SPI — Fortico.
 */
#include "hal/spi/spi_hal.h"

#ifdef ESP32_TARGET
#include <Arduino.h>
#endif

namespace fortico {
namespace hal {

// ============================================================================
// Initialisation
// ============================================================================

bool SpiHal::init(int clkPin, int mosiPin, int misoPin, int csPin)
{
#ifdef ESP32_TARGET
    spi_.begin(clkPin, misoPin, mosiPin, csPin);
    initialized_ = true;
    return true;
#else
    (void)clkPin;
    (void)mosiPin;
    (void)misoPin;
    (void)csPin;
    return false;
#endif
}

// ============================================================================
// Transfert
// ============================================================================

bool SpiHal::transfer(const uint8_t* tx, uint8_t* rx, size_t length)
{
#ifdef ESP32_TARGET
    if (!initialized_ || (tx == nullptr && rx == nullptr) || length == 0U) {
        return false;
    }

    spi_.beginTransaction(
        SPISettings(fortico::hardware::SPI_FREQUENCY_HZ, MSBFIRST, SPI_MODE0));

    for (size_t index = 0U; index < length; ++index) {
        const uint8_t transmitted =
            (tx == nullptr) ? 0xFFU : tx[index];
        const uint8_t received = spi_.transfer(transmitted);
        if (rx != nullptr) {
            rx[index] = received;
        }
    }

    spi_.endTransaction();
    return true;
#else
    (void)tx;
    (void)rx;
    (void)length;
    return false;
#endif
}

// ----------------------------------------------------------------------------

bool SpiHal::write(const uint8_t* tx, size_t length)
{
    return transfer(tx, nullptr, length);
}

// ----------------------------------------------------------------------------

bool SpiHal::read(uint8_t* rx, size_t length)
{
    return transfer(nullptr, rx, length);
}

// ============================================================================
// Accès natif (réservé aux Drivers qui intègrent des libs tierces)
// ============================================================================

#ifdef ESP32_TARGET
SPIClass& SpiHal::native()
{
    return spi_;
}
#endif

}  // namespace hal
}  // namespace fortico
