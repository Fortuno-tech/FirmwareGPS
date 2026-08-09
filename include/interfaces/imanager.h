/**
 * @file    imanager.h
 * @brief   Contrat commun à tout manager de la couche L4 — Fortico.
 *
 * @warning [ÉCART — clarification nécessaire] La proposition initiale
 *          ajoute update()/process() à IManager. Ce n'est PAS anodin :
 *          l'ENSEMBLE du Document 07 (RTOS Design Guide) repose sur un
 *          modèle où CHAQUE manager possède sa propre tâche FreeRTOS dédiée
 *          (RelayManagerTask, AuthenticationManagerTask…), bloquée sur sa
 *          file/event group — PAS un modèle à super-boucle appelant
 *          update() sur tous les managers à chaque tick. Adopter update()/
 *          process() sans le préciser laisserait croire à un changement de
 *          modèle d'exécution que rien ne justifie.
 *
 *          Résolution retenue : update()/process() sont ADOPTÉS mais comme
 *          de simples méthodes internes que la tâche dédiée du manager
 *          appelle elle-même dans SA boucle (document 07 reste inchangé) :
 *          update() = logique périodique/housekeeping (ex. vérification de
 *          timeout), process() = traitement d'un message reçu de la file.
 *          Ce ne sont PAS des hooks appelés depuis l'extérieur par un
 *          ordonnanceur générique.
 */
#pragma once

namespace fortico {
namespace interfaces {

class IManager {
public:
    virtual ~IManager() = default;

    /// Initialisation du manager (Sprint 0 §4.3, inchangé)
    virtual void init() = 0;

    /// [AJOUT, clarifié] Logique périodique interne, appelée par la tâche
    /// FreeRTOS propriétaire du manager (pas par un ordonnanceur externe)
    virtual void update() = 0;

    /// [AJOUT, clarifié] Traitement d'un message/événement reçu, appelé par
    /// la tâche FreeRTOS propriétaire du manager après réception sur sa file
    virtual void process() = 0;

    /// Réinitialisation de l'état interne (ex. après FAULT_SAFE -> reprise)
    virtual void reset() = 0;
};

}  // namespace interfaces
}  // namespace fortico
