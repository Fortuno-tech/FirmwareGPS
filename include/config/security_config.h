/**
 * @file    security_config.h
 * @brief   Structure des clés NVS, chemins de certificats et indicateurs de
 *          sécurité matérielle (Fortico).
 *
 * @details Périmètre strict (Sprint 0, §5) : uniquement des NOMS de clés, des
 *          CHEMINS/SYMBOLES et des indicateurs de fonctionnalité — jamais la
 *          valeur d'un secret. Les valeurs réelles (mots de passe, tokens,
 *          clés privées) sont provisionnées et lues exclusivement via
 *          CredentialsManager depuis la partition NVS chiffrée (Document 06).
 *
 * @note    Source : SOP §4 (R-05 à R-09), §5 (R-12, R-13) ; Document 06
 *          (CredentialsManager) ; Document 09 §2 (clés NVS).
 * @warning Toute Pull Request ajoutant une valeur littérale de secret dans ce
 *          fichier (ou tout autre) est un motif de refus immédiat (Document
 *          01 §4.3 ; Document 03 §6).
 */
#pragma once

#include <cstdint>

namespace fortico {
namespace security {

// ============================================================================
// 1. Espace de noms NVS — Document 09 
// ============================================================================

/// Namespace NVS dédié à l'état et aux secrets Fortico
constexpr const char* NVS_NAMESPACE = "fortico";

// ----------------------------------------------------------------------------
// Clés NVS — noms uniquement, jamais de valeur
// ----------------------------------------------------------------------------

/// Dernier état connu du relais (R-11) — persistance immédiate à chaque changement
constexpr const char* NVS_KEY_RELAY_STATE = "relay_state";

/// Identifiants Wi-Fi chiffrés
constexpr const char* NVS_KEY_WIFI_CREDS = "wifi_creds";

/// Identifiants MQTT chiffrés
constexpr const char* NVS_KEY_MQTT_CREDS = "mqtt_creds";

/// Identifiants APN chiffrés
constexpr const char* NVS_KEY_APN_CREDS = "apn_creds";

/// Liste blanche des UID RFID autorisés
constexpr const char* NVS_KEY_RFID_WHITELIST = "rfid_whitelist";

/// Compteur de séquence télémétrie 
constexpr const char* NVS_KEY_DEVICE_SEQ = "device_seq";

/// Horodatage de la dernière vérification de signature OTA
constexpr const char* NVS_KEY_OTA_LAST_SIG_CHECK = "ota_last_sig_check";

/// Curseur de synchronisation du backlog boîte noire
constexpr const char* NVS_KEY_LAST_SYNCED_SEQ = "last_synced_seq";

/// [NOUVEAU] Token JWT device courant (authentification boîtier auprès du
/// cloud, AuthenticationService — distinct de NVS_KEY_RFID_WHITELIST qui
/// concerne l'authentification du conducteur, portée par
/// AuthenticationManager, document 06)
constexpr const char* NVS_KEY_JWT_DEVICE_TOKEN = "jwt_device_token";

/// [NOUVEAU] Clé secrète de signature/vérification JWT device, si schéma
/// symétrique retenu (HMAC) — à confirmer avec l'équipe backend
constexpr const char* NVS_KEY_JWT_SIGNING_KEY = "jwt_signing_key";

// ============================================================================
// 2. Transport sécurisé — SOP §4.1 (R-05, R-06)
// ============================================================================

/// Symbole du certificat CA Fortico embarqué (assets/, lié au binaire)
constexpr const char* CA_CERTIFICATE_SYMBOL = "fortico_ca_cert_pem";

/// Version minimale de protocole TLS acceptée
constexpr const char* TLS_MIN_VERSION = "TLS1.2";

// ============================================================================
// 3. OTA signé — SOP §5 (R-13)
// ============================================================================

/// Symbole de la clé publique de vérification de signature OTA (assets/)
constexpr const char* OTA_PUBLIC_KEY_SYMBOL = "fortico_ota_pubkey_pem";

/// Algorithme de signature attendu pour tout binaire OTA
constexpr const char* OTA_SIGNATURE_ALGORITHM = "Ed25519";

// ============================================================================
// 3bis. Chiffrement symétrique applicatif — SecurityService (document 05,
//       nouveau composant L3) — encrypt()/decrypt() sur données au repos ou
//       en transit hors TLS (ex. champs sensibles en boîte noire SD)
// ============================================================================

/// Algorithme de chiffrement symétrique utilisé par SecurityService
constexpr const char* SYMMETRIC_CIPHER_ALGORITHM = "AES-256-GCM";

// ============================================================================
// 4. Indicateurs de sécurité matérielle — SOP §4.2 (R-08)
// ============================================================================

/// Indicateur de compilation : Flash Encryption attendue active en production
/// (activée via sdkconfig / menuconfig PlatformIO, pas ici)
constexpr bool FLASH_ENCRYPTION_REQUIRED_IN_PRODUCTION = true;

/// Indicateur de compilation : Secure Boot attendu actif en production
constexpr bool SECURE_BOOT_REQUIRED_IN_PRODUCTION = true;

// ============================================================================
// 5. Anti-rejeu des commandes distantes — Document 08 §2 [ASSUMPTION]
// ============================================================================

/// Fenêtre de validité maximale d'une commande MQTT entrante avant rejet
constexpr uint32_t COMMAND_VALIDITY_WINDOW_S = 60;

}  // namespace security
}  // namespace fortico