/**
 * @file    pin_config.h
 * @brief   Source de vérité UNIQUE pour toute affectation de broche ESP32 (Fortico).
 *
 * @details Aucun driver, manager ou service ne doit écrire un numéro de GPIO en
 *          dur ailleurs que via les constantes de ce fichier.
 *          Voir Document 03 (Coding Standard, §3), Document 10 (Hardware
 *          Integration Guide) et Sprint 0 (§5 — Fichiers de configuration).
 *
 * @note    Sources faisant foi :
 *            - Cartographie révisée des broches ESP32 (V4) pour tout GPIO déjà
 *              affecté avant ce document.
 *            - Schéma d'architecture logicielle « vue_par_couche » (couches
 *              Managers/Drivers), qui réintroduit un Fuel Manager / Fuel Driver
 *              (ADC) absent du Pinout V4 — voir résolution en §9.4.
 *            - Changement matériel (rév. courante) : le module GPS NEO-6M
 *              autonome est retiré. Le SIM7600E-L1C intègre son propre moteur
 *              GNSS (antenne GPS dédiée sur le breakout, distincte de
 *              l'antenne 4G) et fournit la position via commandes AT
 *              (AT+CGPS / AT+CGPSINFO) sur SON PROPRE canal UART — voir §5.
 *
 * @warning Toute modification de ce fichier doit être répercutée dans le
 *          Document 10 avant merge de la Pull Request correspondante.
 */
#pragma once

#include <cstdint>

namespace fortico {
namespace pins {

// ============================================================================
// 1. RÉSERVÉ — Console de programmation / débogage série
//    Ne JAMAIS réaffecter ces broches à un périphérique applicatif.
// ============================================================================

/// Console série (flash / moniteur), réservé
constexpr uint8_t UART0_TX = 1;
/// Console série (flash / moniteur), réservé
constexpr uint8_t UART0_RX = 3;

// ============================================================================
// 2. Bus I2C partagé — RTC DS3231 + RFID PN532
//    Bus non exclusif : tout accès doit passer par i2cBusMutex
//    (Document 07 §5 ; Document 04 §4). Pull-up externes 4.7 kΩ obligatoires
//    si non intégrées aux modules (Pinout V4, note d'intégration n°2).
// ============================================================================

/// Bus I2C — SDA, partagé entre rtcManager et rfidManager (PN532)
constexpr uint8_t I2C_SDA = 21;
/// Bus I2C — SCL, partagé entre rtcManager et rfidManager (PN532)
constexpr uint8_t I2C_SCL = 22;

// ============================================================================
// 3. Bus SPI exclusif — Carte SD
//    Aucun autre périphérique ne doit être posé sur ce bus (Document 04 §1).
// ============================================================================

/// Carte SD — Chip Select
constexpr uint8_t SD_CS = 4;
/// Carte SD — Horloge SPI (VSPI)
constexpr uint8_t SD_SCK = 18;
/// Carte SD — Données SPI, maître vers esclave
constexpr uint8_t SD_MOSI = 23;
/// Carte SD — Données SPI, esclave vers maître
constexpr uint8_t SD_MISO = 19;

// ============================================================================
// 4. [SUPPRIMÉ] GPS NEO-6M autonome — voir §5
// ----------------------------------------------------------------------------
//    Le module NEO-6M et son UART2 dédié (anciennement GPS_RX2=16,
//    GPS_TX2=17) sont retirés de l'architecture matérielle. Le SIM7600E-L1C
//    intègre désormais la fonction GPS/GNSS. GPIO16 et GPIO17 sont donc
//    LIBÉRÉS — voir §7 (réserve d'évolution).
// ============================================================================

/// SIM7600E-L1C — UART1 TX (canal AT partagé : cellulaire + GNSS)
constexpr uint8_t SIM7600_TX1 = 27;
/// SIM7600E-L1C — UART1 RX (canal AT partagé : cellulaire + GNSS)
constexpr uint8_t SIM7600_RX1 = 14;

// ============================================================================
// 6. Actionneur critique — Relais de coupure moteur (5V, 20A)
//    RelayManager est l'UNIQUE propriétaire logiciel autorisé à écrire cette
//    broche (Document 06 §2 ; Document 02 §4 — machine à états système).
// ============================================================================

/// Commande bobine du relais de coupure moteur — écriture exclusive RelayManagerTask
constexpr uint8_t RELAY_CTRL = 12;

// ============================================================================
// 7. Interface d'alerte cabine — Buzzer / LED
// ============================================================================

/// Buzzer d'alarme cabine
constexpr uint8_t BUZZER_SIGNAL = 13;
/// LED témoin d'état système
constexpr uint8_t LED_INDICATOR = 33;

// ============================================================================
// 8. Entrée utilisateur — Bouton poussoir (alerte SOS)
//    Anti-rebond logiciel obligatoire ; distinction appui court / long
//    (Document 06 — AlertManager).
// ============================================================================

/// Bouton poussoir SOS
constexpr uint8_t BUTTON_SOS = 25;

// ============================================================================
// 9. Entrées analogiques (ADC1 — utilisables simultanément avec le Wi-Fi,
//    contrairement à ADC2)
// ============================================================================

/// Tension batterie véhicule (12V nominal), ADC1_CH6 — pont diviseur obligatoire
/// (Pinout V4, note d'intégration n°3)
constexpr uint8_t BATTERY_ADC = 34;

/// Sonde de température, ADC1_CH4 
constexpr uint8_t TEMPERATURE_ADC = 32;

/**
 * @brief Sonde carburant (jauge résistive/capacitive en citerne) — ADC1_CH0.
 *
 * @warning [ASSUMPTION — ARBITRAGE HARDWARE REQUIS AVANT ROUTAGE PCB]
 *          Le schéma « vue_par_couche » réintroduit un Fuel Manager / Fuel
 *          Driver (ADC), conforme à la vision d'origine (pilier « détection de
 *          vol de carburant »), alors que le Pinout V4 avait supprimé ce point
 *          d'entrée matériel en réaffectant GPIO32 à la température.
 *          Résolution retenue : nouvelle broche ADC libre plutôt que de
 *          sacrifier la sonde de température déjà câblée. GPIO36 choisi car
 *          libre, ADC1 pur, compatible Wi-Fi, entrée pure.
 *          À valider avant routage PCB : choix de broche + conditionnement du
 *          signal (pont diviseur / filtrage). Voir Document 10 §5.
 */
constexpr uint8_t FUEL_ADC = 36;

// ============================================================================
// 10. Broches libres restantes — réserve d'évolution
// ============================================================================

/// Libérée suite au passage du RFID en I2C
constexpr uint8_t SPARE_GPIO_2 = 2;
/// Libérée suite au passage du RFID en I2C
constexpr uint8_t SPARE_GPIO_5 = 5;
/// Libérée suite au déplacement de la LED en GPIO33
constexpr uint8_t SPARE_GPIO_15 = 15;

constexpr uint8_t SPARE_GPIO_16 = 16;

constexpr uint8_t SPARE_GPIO_17 = 17;
/// Dispose d'une sortie DAC2 intégrée
constexpr uint8_t SPARE_GPIO_26 = 26;
/// ADC1_CH7, entrée pure
constexpr uint8_t SPARE_ADC_35 = 35;
/// ADC1_CH3, entrée pure
constexpr uint8_t SPARE_ADC_39 = 39;

}  // namespace pins
}  // namespace fortico