/**
 * @file    relay_types.h
 * @brief   États et commandes du relais de coupure moteur — Fortico.
 *
    * @note    RelayState et RelayCmd sont distincts : une commande ENGAGE amène l'état à ENGAGED, etc.
 */
#pragma once

#include <cstdint>

namespace fortico {
namespace types {

/// État détenu par RelayManager (Document 02 §5.2) — PAS un état
/// AppStateMachine global.
enum class RelayState {
    LOCKED,          ///< relais CUT, aucun badge valide
    ENGAGED,          ///< relais ENGAGE, badge valide, démarrage autorisé
    EMERGENCY_CUT,    ///< relais CUT forcé — nécessite ré-authentification pour sortir de cet état
};

/// Commande transmise via q_relayCmd (Document 07 §3) — distincte de
/// RelayState : une commande ENGAGE amène l'état à ENGAGED, etc.
enum class RelayCmd {
    ENGAGE,
    CUT,
    EMERGENCY_CUT,
};

/// Origine de la commande, tracée en boîte noire (Document 06 §2.1)
enum class RelaySource : uint8_t {
    AUTH,
    BUTTON_SOS,
    REMOTE_COMMAND,
    FAULT,
};

struct RelayCommand {
    RelayCmd cmd;
    RelaySource source;
    uint64_t timestamp;
};

}  // namespace types
}  // namespace fortico
