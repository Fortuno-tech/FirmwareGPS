/**
 * @file    ievent_listener.h
 * @brief   Contrat de réception d'événement EventBus — Fortico.
 *
 * @note    [ÉCART] onEvent(uint32_t eventId) dans la proposition initiale ->
 *          onEvent(const types::Event&) ici. Un simple uint32_t perdrait le
 *          timestamp et l'auxData déjà nécessaires ailleurs (Document 06 —
 *          toute alerte/événement journalisé porte un horodatage, Document
 *          09 §7). Voir types/system_types.h pour la définition d'Event.
 */
#pragma once

#include "types/system_types.h"

namespace fortico {
namespace interfaces {

class IEventListener {
public:
    virtual ~IEventListener() = default;
    virtual void onEvent(const types::Event& event) = 0;
};

}  // namespace interfaces
}  // namespace fortico
