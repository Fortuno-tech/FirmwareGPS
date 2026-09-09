#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H

// =====================================================
// FROTICO - PIN CONFIGURATION
// =====================================================
#define DHT_PIN 33
#define DHT_TYPE DHT22
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
#define SIM7600_RX_PIN 25   // GPIO25 (RX de l'ESP) <- T du SIM7600
#define SIM7600_TX_PIN 32   // GPIO32 (TX de l'ESP) -> R du SIM7600
#define SIM7600_PWRKEY_PIN 4 // GPIO4 -> K du SIM7600

#define DS3231_I2C_ADDRESS 0x68

#endif