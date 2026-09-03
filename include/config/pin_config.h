#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H

// =====================================================
// FROTICO - PIN CONFIGURATION
// =====================================================

// -------------------------
// I2C
// -------------------------
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

// -------------------------
// PN532
// -------------------------
#define PN532_IRQ_PIN 27
#define PN532_RESET_PIN 26

// =====================================================
// SIM7600 UART
// =====================================================

#define SIM7600_RX_PIN 16
#define SIM7600_TX_PIN 17

#define SIM7600_PWRKEY_PIN 4

#define DS3231_I2C_ADDRESS 0x68

#endif