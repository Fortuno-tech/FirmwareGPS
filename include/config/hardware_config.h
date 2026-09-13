/**
 * @file    hardware_config.h
 * @brief   Frequences de bus, timeouts et parametres de calibration/filtrage
 *          materiel (Fortico).
 */
#pragma once

#include <cstdint>

namespace fortico {
namespace hardware {

// ============================================================================
// 1. Bus I2C partage (RTC DS3231 + RFID PN532) 
// ============================================================================

/// Frequence du bus I2C (100 kHz standard, optimal pour PN532 + DS3231 sur ESP32)
constexpr uint32_t I2C_FREQUENCY_HZ = 100000;

/// Timeout maximal de prise du mutex i2cBusMutex avant abandon de la transaction
constexpr uint32_t I2C_MUTEX_TIMEOUT_MS = 100;

/// Duree maximale d'une transaction I2C sous mutex
constexpr uint32_t I2C_TRANSACTION_MAX_MS = 100;

constexpr uint32_t RFID_POLL_INTERVAL_MS = 300;
/// Attente maximale du PN532 pour une tentative RFID.
constexpr uint32_t RFID_READ_TIMEOUT_MS = 500;
constexpr uint32_t RTC_POLL_INTERVAL_MS = 1800000UL;
/// Vérification/synchronisation RTC depuis le réseau cellulaire toutes les 2 h.
constexpr uint32_t NETWORK_TIME_SYNC_INTERVAL_MS = 7200000UL;

/// Nombre d'echecs I2C consecutifs avant de declarer le peripherique
/// PERIPH_DEGRADED (RTC ou RFID)
constexpr uint8_t I2C_MAX_CONSECUTIVE_FAILURES = 5;

// ============================================================================
// 2. Bus SPI exclusif (carte SD)
// ============================================================================

constexpr std::uint8_t DRIVER_MAX_CONSECUTIVE_FAILURES = 5U;
constexpr uint32_t SPI_FREQUENCY_HZ = 4000000;

// ============================================================================
// 3. Liaison UART - Modem SIM7600E-L1C
// ============================================================================

constexpr uint32_t SIM7600_UART_BAUD_RATE = 115200;
constexpr uint32_t SIM7600_AT_TIMEOUT_MS = 2000;
constexpr uint32_t SIM7600_AT_TIMEOUT_LONG_MS = 10000;
constexpr uint8_t SIM7600_AT_MAX_ATTEMPTS = 5U;
constexpr uint32_t SIM7600_AT_RETRY_DELAY_MS = 2000U;
constexpr bool SIM7600_PWRKEY_IDLE_LEVEL = true;
constexpr uint32_t SIM7600_PWRKEY_SETTLE_MS = 100U;
constexpr uint32_t SIM7600_PWRKEY_PULSE_MS = 1500U;
constexpr uint32_t SIM7600_BOOT_DELAY_MS = 5000U;
constexpr uint32_t SIM7600_GNSS_QUERY_TIMEOUT_MS = 3000;
constexpr uint32_t SIM7600_UART_MUTEX_TIMEOUT_MS = 50;
constexpr uint32_t GNSS_POLL_INTERVAL_MS = 1000;
constexpr uint32_t GPS_LAST_FIX_MAX_AGE_MS = 15000;
constexpr uint8_t GPS_MIN_SATELLITES_FOR_FIX = 4;

// ============================================================================
// 4. Echantillonnage et filtrage ADC
// ============================================================================

constexpr uint8_t ADC_ATTENUATION_DB = 11;
constexpr float FUEL_MIN_VALID_VOLTAGE = 0.05F;
constexpr float FUEL_MAX_VALID_VOLTAGE = 3.35F;
constexpr uint8_t ADC_OVERSAMPLE_COUNT = 16;
constexpr float ADC_EMA_ALPHA = 0.2f;
constexpr uint32_t SENSOR_SAMPLING_PERIOD_MS = 500;

#include "utils/adc_calibration.h"

constexpr float BATTERY_VOLTAGE_DIVIDER_RATIO = fortico::calibration::BATTERY_DIVIDER_RATIO;
constexpr float BATTERY_CRITICAL_VOLTAGE = 10.5f;
constexpr float FUEL_SENDER_RATIO_PLACEHOLDER = fortico::calibration::FUEL_SENDER_DIVIDER_RATIO;

// ============================================================================
// 5. Entrees utilisateur
// ============================================================================

constexpr uint32_t BUTTON_DEBOUNCE_MS = 30;
constexpr uint32_t BUTTON_LONG_PRESS_MS = 2000;

// ============================================================================
// 6. Watchdog materiel
// ============================================================================

constexpr uint32_t WDT_TIMEOUT_S = 8;

// ============================================================================
// 7. Stockage local (carte SD)
// ============================================================================

constexpr uint32_t SD_FLUSH_INTERVAL_MS = 5000;
constexpr uint16_t SD_WRITE_BUFFER_MAX_RECORDS = 64;

}  // namespace hardware
}  // namespace fortico
