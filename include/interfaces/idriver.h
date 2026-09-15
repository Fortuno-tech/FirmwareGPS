/**
 * @file    idriver.h
 * @brief   Contrat commun à tout driver de la couche L2 — Fortico.
 *
 * @warning  La proposition initiale suggérait
 *          init()/start()/stop()/reset()/isInitialized(), SANS poll() ni
 *          isHealthy(). Ce n'est PAS retenu tel quel : poll() et isHealthy()
 *          sont au cœur de la tolérance de panne exigée par la SOP (§3.2,
 *          R-03) et sont référencés dans TOUT le Document 04 (ex. §3, §4,
 *          §7 : "isHealthy() implémenté et exploitable par le manager
 *          appelant" fait partie de la checklist de recevabilité d'un
 *          driver). Les supprimer casserait la cohérence de ce document déjà
 *          livré et le mécanisme PERIPH_DEGRADED qui gouverne la dégradation
 *          gracieuse partout dans l'architecture.
 *
 *          Résolution retenue : FUSION. start()/stop() et isInitialized()
 *          de la proposition sont des AJOUTS utiles (contrôle de cycle de
 *          vie pour PowerManager, ex. couper l'alimentation d'un capteur en
 *          SLEEP) et sont intégrés SANS retirer poll()/isHealthy().
 */
#pragma once

#include "constants/errors.h"

namespace fortico {
namespace interfaces {

class IDriver {
public:
    virtual ~IDriver() = default;

    /// Configuration initiale du bus/registre, sans blocage (Document 04 §2)
    virtual constants::ErrorCode init() = 0;

    /// Appelée périodiquement par la tâche propriétaire (Document 04 §2, 07)
    virtual constants::ErrorCode poll() = 0;

    ///  Active le périphérique après un stop() — ex. sortie de SLEEP
    virtual bool start() = 0;

    ///  Désactive le périphérique sans perdre son état interne — ex. entrée en SLEEP (PowerManager, stretch goal)
    virtual bool stop() = 0;

    /// État de santé courant — fondement de la tolérance de panne (SOP §3.2, Document 04 §3)
    virtual bool isHealthy() const = 0;

    /// Vrai si init() a réussi au moins une fois — distinct d'isHealthy()
    /// (un driver initialisé peut devenir temporairement malsain sans avoir besoin d'un nouveau init())
    virtual bool isInitialized() const = 0;

    /// Tentative de récupération logicielle (Document 04 §3)
    virtual void reset() = 0;
};

}  // namespace interfaces
}  // namespace fortico
