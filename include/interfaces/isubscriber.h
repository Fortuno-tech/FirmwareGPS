/**
 * @file    isubscriber.h
 * @brief   Contrat d'enregistrement/désenregistrement auprès de l'EventBus —
 *          Fortico.
 *
 * @warning [ÉCART — faille de conception corrigée] La proposition initiale
 *          donnait à ISubscriber une méthode notify(const Event&), IDENTIQUE
 *          en pratique à IEventListener::onEvent(). Cela crée une
 *          redondance fonctionnelle et surtout NE FOURNIT AUCUN MOYEN DE
 *          S'ABONNER : avec ce design, rien dans le contrat ne permet à un
 *          composant de dire "je veux recevoir tel type d'événement". Un
 *          ISubscriber doit être l'interface de L'ENTITÉ QUI GÈRE LES
 *          ABONNEMENTS (typiquement EventBus lui-même), pas un doublon du
 *          récepteur. Repris de la spécification Sprint 0 §4.3, qui avait
 *          déjà cette forme correcte.
 *
 * @note    [CHANGÉ 13/07/2026] Le topic std::string initial est remplacé par
 *          constants::EventID directement. EventID est déjà un enum unique
 *          par type d'événement — lui faire jouer le rôle de clé de routage
 *          évite une couche de correspondance supplémentaire (chaîne <->
 *          EventID), la comparaison de chaînes (strcmp à chaque dispatch),
 *          et toute possibilité de faute de frappe entre publish() (qui
 *          connaît l'EventID) et subscribe() (qui aurait connu une chaîne
 *          distincte). Permet aussi à EventBus d'indexer directement son
 *          tableau d'abonnements par EventID (accès O(1), voir
 *          constants/events.h et event_bus.h).
 */
#pragma once

#include "interfaces/ievent_listener.h"
#include "constants/events.h"

namespace fortico {
namespace interfaces {

class ISubscriber {
public:
    virtual ~ISubscriber() = default;
    virtual void subscribe(constants::EventID id, IEventListener* listener) = 0;
    virtual void unsubscribe(constants::EventID id, IEventListener* listener) = 0;
};

}  // namespace interfaces
}  // namespace fortico