/**
 * @file    iservice.h
 * @brief   Contrat commun à tout service de la couche L3 — Fortico.
 *
 * @note    [AMÉLIORATION ADOPTÉE] start() retourne désormais bool (détection
 *          d'échec de démarrage) et isRunning() est ajouté, par rapport à la
 *          version minimale du Sprint 0 §4.3 (start()/stop() en void
 *          uniquement). Aucun conflit avec un document déjà livré — adopté
 *          tel que proposé.
 */
#pragma once

namespace fortico {
namespace interfaces {

class IService {
public:
    virtual ~IService() = default;

    /// Démarre le service (ex. ouverture de connexion, création de tâche interne). Retourne faux en cas d'échec immédiat.
    virtual bool start() = 0;

    /// Arrête proprement le service
    virtual void stop() = 0;

    /// État courant — consulté avant toute opération dépendante (ex. MqttService::isRunning() avant publish)
    virtual bool isRunning() const = 0;
};

}  // namespace interfaces
}  // namespace fortico
