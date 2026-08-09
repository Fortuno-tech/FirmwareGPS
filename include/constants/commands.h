/**
 * @file    commands.h
 * @brief   Commandes distantes (MQTT topic cmd) et commandes internes —
 *          Fortico.
 *
 * @note    Aligné sur le schéma déjà publié au Document 09 §5 (FORCE_CUT,
 *          FORCE_ENGAGE, OTA_TRIGGER, WHITELIST_UPDATE), PAS sur la
 *          proposition initiale (ENGINE_LOCK/ENGINE_UNLOCK) qui contredisait
 *          un document déjà livré. REQUEST_POSITION et REBOOT ajoutés
 *          (absents du Document 09, extension raisonnable).
 */
#pragma once

namespace fortico {
namespace constants {

enum class Command {
    FORCE_CUT,          // Document 09 coupure moteur distante authentifiée
    FORCE_ENGAGE,        // Document 09  soumis à confirmation locale d'arrêt (Document 02 §5.2)
    OTA_TRIGGER,         // Document 09 
    WHITELIST_UPDATE,     // Document 09 mise à jour liste blanche RFID
    REQUEST_POSITION,     // demande de position immédiate hors cycle normal
    REBOOT,              //  redémarrage contrôlé à distance
};

}  // namespace constants
}  // namespace fortico
