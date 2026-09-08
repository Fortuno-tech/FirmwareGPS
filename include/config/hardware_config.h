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

#define SIM7600_MAX_AT_ATTEMPTS   5

#define SIM7600_APN ""

#define SIM7600_NETWORK_CHECK_INTERVAL_MS 5000
#define SIM7600_NETWORK_SEARCH_TIMEOUT_MS 60000
#define SIM7600_PDP_CONTEXT 1
#endif