/**
 * @file    errors.h
 * @brief   Codes d'erreur communs à tous les drivers/managers/services —
 *          Fortico.
 *
 * @note    Renommé par rapport à la proposition initiale : GPS_TIMEOUT ->
 *          GNSS_TIMEOUT, cohérent avec la fusion GPS/SIM7600 (Document 04
 *          §5, Document 10 §5.4 — il n'y a plus de driver GPS autonome).
 */
#pragma once

#include <cstdint>

namespace fortico {
namespace constants {

enum class ErrorCode : uint8_t {
    OK = 0,

    // Sim7600Driver (cellulaire + GNSS, Document 04 §5)
    GNSS_TIMEOUT,
    MODEM_AT_TIMEOUT,
    MODEM_REGISTRATION_FAILED,
    MQTT_TIMEOUT,
    MQTT_CONNECT_FAILED,
    NETWORK_FAILURE,

    // Bus partagés (Document 04 §4)
    I2C_BUS_TIMEOUT,
    I2C_BUS_BUSY,

    // Drivers spécifiques
    RTC_FAILURE,
    RFID_FAILURE,
    SD_FAILURE,
    SD_NOT_MOUNTED,
    RELAY_FAULT,
    FUEL_SENSOR_FAILURE,
    BATTERY_SENSOR_FAILURE,
    TEMPERATURE_SENSOR_FAILURE,

    // Sécurité / OTA (SOP §4, §5)
    OTA_SIGNATURE_INVALID,
    OTA_DOWNLOAD_FAILED,
    NVS_WRITE_FAILED,
    NVS_READ_FAILED,

    // Générique
    TIMEOUT,
    INVALID_PARAM,
    NOT_INITIALIZED,
    UNKNOWN,
};

}  // namespace constants
}  // namespace fortico
