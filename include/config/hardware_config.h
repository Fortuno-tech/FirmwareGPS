#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

// =====================================================
// FROTICO - HARDWARE CONFIGURATION
// =====================================================

// Debug UART
#define DEBUG_BAUDRATE 115200

// I2C
#define I2C_FREQUENCY 100000UL
#define I2C_TIMEOUT_MS 100UL

// =====================================================
// SIM7600
// =====================================================

#define SIM7600_BAUDRATE 115200

#define SIM7600_UART_NUMBER       2

#define SIM7600_AT_TIMEOUT_MS     3000

#define SIM7600_BOOT_DELAY_MS     5000

#define SIM7600_INFO_TIMEOUT_MS   5000

#define SIM7600_NETWORK_TIMEOUT_MS 3000

#define SIM7600_AT_ATTEMPTS                 5
#define SIM7600_APN                         "internet"
#define SIM7600_PDP_TYPE                    "IP"
#define SIM7600_DEFAULT_IP                   "0.0.0.0"
#define SIM7600_PING_IP                      "8.8.8.8"
#define SIM7600_PING_TIMEOUT_MS               10000
#define SIM7600_NETWORK_CHECK_INTERVAL_MS 5000
#define SIM7600_NETWORK_SEARCH_TIMEOUT_MS 180000
#define SIM7600_PDP_CONTEXT 1


#define SIM7600_GPS_ENABLE              true
#define GPS_UPDATE_INTERVAL_MS          5000
#define GPS_FIX_CHECK_INTERVAL_MS       2000
#define GPS_START_TIMEOUT_MS            120000
#define GPS_INFO_TIMEOUT_MS             5000
#endif