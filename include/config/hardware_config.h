/**
 * @file    hardware_config.h
 * @brief   Fréquences de bus, timeouts et paramètres de calibration/filtrage
 *          matériel (Fortico).
 *
 * @details Périmètre strict (Sprint 0, §5) : uniquement les paramètres
 *          électriques/temporels des bus et capteurs. L'affectation des GPIO
 *          reste dans pin_config.h ; les priorités/tailles de tâches FreeRTOS
 *          sont dans task_config.h.
 *
 * @note    Sources : Document 04 (Driver Development Guide), Document 02 §6
 *          (échantillonnage ADC), Document 10 (Hardware Integration Guide).
 */
#pragma once

#include <cstdint>

namespace fortico {
namespace hardware {

// ============================================================================
// 1. Bus I2C partagé (RTC DS3231 + RFID PN532) 
// ============================================================================

/// Fréquence du bus I2C
constexpr uint32_t I2C_FREQUENCY_HZ = 400000;

/// Timeout maximal de prise du mutex i2cBusMutex avant abandon de la
/// transaction 
constexpr uint32_t I2C_MUTEX_TIMEOUT_MS = 50;

/// Durée cible maximale d'une transaction I2C sous mutex
constexpr uint32_t I2C_TRANSACTION_MAX_MS = 20;

/// Nombre d'échecs I2C consécutifs avant de déclarer le périphérique
/// PERIPH_DEGRADED (RTC ou RFID)
constexpr uint8_t I2C_MAX_CONSECUTIVE_FAILURES = 5;

// ============================================================================
// 2. Bus SPI exclusif (carte SD) — Document 04
// ============================================================================

/// Fréquence du bus SPI (VSPI) dédié à la carte SD
constexpr uint32_t SPI_FREQUENCY_HZ = 4000000;

// ============================================================================
// 3. Liaison UART — Modem SIM7600E-L1C (cellulaire + GNSS intégré)
//    Le NEO-6M autonome et son UART2 dédié sont retirés — voir pin_config.h §4-5.
// ============================================================================

/// Débit UART1 — Modem SIM7600E-L1C (canal AT partagé : cellulaire + GNSS,
/// voir pin_config.h §5)
constexpr uint32_t SIM7600_UART_BAUD_RATE = 115200;

/// Timeout par défaut d'une commande AT envoyée au SIM7600 avant statut TIMEOUT
constexpr uint32_t SIM7600_AT_TIMEOUT_MS = 2000;

/// Timeout étendu pour les commandes AT longues (ex. attache réseau, contexte PDP)
constexpr uint32_t SIM7600_AT_TIMEOUT_LONG_MS = 10000;

/**
 * @brief Timeout de la commande d'interrogation de position GNSS
 *        (AT+CGPSINFO / AT+CGNSSINFO), plus longue qu'une commande AT
 *        classique tant que le moteur GNSS n'a pas de premier fix (cold
 *        start possible).
 * @note  [NOUVEAU] Introduit avec l'intégration GNSS dans le SIM7600E-L1C
 *        (remplace le NEO-6M autonome) — voir pin_config.h §5.
 */
constexpr uint32_t SIM7600_GNSS_QUERY_TIMEOUT_MS = 3000;

/**
 * @brief Timeout de prise du mutex du canal UART/AT du SIM7600, partagé
 *        entre les commandes cellulaires (NetworkService/MQTTService) et
 *        les commandes GNSS (TrackingManager). Même logique d'arbitrage que
 *        i2cBusMutex (Document 04 §4), appliquée ici à sim7600UartMutex.
 * @warning [NOUVEAU] Avant l'intégration GNSS, sim7600UartMutex n'était
 *          utilisé qu'en interne par sim7600Manager (Document 07 §5). Il
 *          devient désormais une ressource explicitement partagée entre
 *          deux consommateurs métier distincts — à répercuter dans le
 *          Document 07 (table des mutex) et le Document 04 (driver SIM7600).
 */
constexpr uint32_t SIM7600_UART_MUTEX_TIMEOUT_MS = 50;

/// Période d'interrogation GNSS en régime nominal (TRACKING_ONLINE/OFFLINE)
constexpr uint32_t GNSS_POLL_INTERVAL_MS = 1000;

/// Nombre minimal de satellites GPS pour valider un fix (Document 06 §4)
/// [ASSUMPTION — seuil non fourni dans les documents sources]
constexpr uint8_t GPS_MIN_SATELLITES_FOR_FIX = 4;

// ============================================================================
// 4. Échantillonnage et filtrage ADC — Document 02 §6, Document 10 §7
// ============================================================================

/// Atténuation ADC (11 dB ~ plage utile 0–3,3 V après pont diviseur)
constexpr uint8_t ADC_ATTENUATION_DB = 11;

/// Nombre d'échantillons moyennés par cycle de lecture (sur-échantillonnage)
constexpr uint8_t ADC_OVERSAMPLE_COUNT = 16;

/// Coefficient du filtre passe-bas exponentiel (EMA) appliqué après la moyenne
constexpr float ADC_EMA_ALPHA = 0.2f;

/// Période d'échantillonnage de SensorSamplingTask (batterie/température/carburant)
constexpr uint32_t SENSOR_SAMPLING_PERIOD_MS = 500;

/**
 * @brief Ratio du pont diviseur de tension batterie (12V nominal).
 * @warning Valeur d'exemple — DOIT être calibrée physiquement sur le montage
 *          réel avant toute mise en production (Pinout V4, note n°3).
 */
constexpr float BATTERY_VOLTAGE_DIVIDER_RATIO = 4.0f;

/// Seuil de tension batterie considéré critique (déclenche DEGRADED_MODE,
/// jamais une coupure moteur — Document 02 §4) [ASSUMPTION seuil]
constexpr float BATTERY_CRITICAL_VOLTAGE = 10.5f;

/**
 * @brief Ratio de conversion de la jauge carburant (FUEL_ADC, GPIO36).
 * @warning [ASSUMPTION] Broche et conditionnement provisoires — voir
 *          pin_config.h §9.3. Ratio à déterminer une fois le type de jauge
 *          (résistive/capacitive) confirmé par l'équipe hardware.
 */
constexpr float FUEL_SENDER_RATIO_PLACEHOLDER = 1.0f;

// ============================================================================
// 5. Entrées utilisateur — Document 06 (AlertManager)
// ============================================================================

/// Anti-rebond logiciel du bouton SOS
constexpr uint32_t BUTTON_DEBOUNCE_MS = 30;

/// Durée d'appui minimale pour qualifier un « appui long » (SOS)
constexpr uint32_t BUTTON_LONG_PRESS_MS = 2000;

// ============================================================================
// 6. Watchdog matériel — SOP §3.1 (R-02), Document 07 §6
// ============================================================================

/// Timeout du watchdog matériel avant reset automatique
constexpr uint32_t WDT_TIMEOUT_S = 8;

// ============================================================================
// 7. Stockage local (carte SD) — Document 09 §7
// ============================================================================

/// Intervalle de flush périodique du buffer d'écriture SD
constexpr uint32_t SD_FLUSH_INTERVAL_MS = 5000;

/// Taille du buffer RAM d'enregistrements avant flush forcé [ASSUMPTION]
constexpr uint16_t SD_WRITE_BUFFER_MAX_RECORDS = 64;

}  // namespace hw
}  // namespace fortico