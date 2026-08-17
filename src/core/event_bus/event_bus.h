/**
 * @file    event_bus.h
 * @brief   EventBus — hub de publication/souscription centralisé (L0) —
 *          Fortico.
 *
 * @details Implémente IPublisher et ISubscriber (include/interfaces/).
 *          Fait tourner sa propre tâche FreeRTOS (EventBusTask) qui dépile
 *          qEventBusIn et dispatche chaque événement vers les
 *          IEventListener abonnés au topic correspondant.
 *
 * @warning CONTRAT STRICT pour tout abonné : IEventListener::onEvent() DOIT
 *          être non bloquant et court (typiquement un simple xQueueSend()
 *          vers la file interne du manager) — jamais de traitement métier
 *          dans le contexte d'EventBusTask. Voir Document 07 §4bis.1.
 *
 * @warning EXCEPTION DÉLIBÉRÉE : q_relayCmd n'est JAMAIS alimentée via
 *          EventBus. RelayManager ne s'abonne à rien ici — les commandes
 *          relais restent des écritures directes de queue à queue
 *          (Document 07 §4bis.1), pour ne pas ajouter de latence sur le
 *          chemin le plus critique du firmware.
 *
 * @note    Aucune allocation dynamique (Document 03 §5) : la table
 *          d'abonnements est un tableau statique borné par
 *          EVENT_BUS_MAX_TOPICS / EVENT_BUS_MAX_LISTENERS_PER_TOPIC
 *          (task_config.h).
 */
#pragma once

#include <cstdint>
#include <cstring>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include "interfaces/ipublisher.h"
#include "interfaces/isubscriber.h"
#include "interfaces/ievent_listener.h"
#include "types/system_types.h"
#include "constants/events.h"
#include "config/task_config.h"

namespace fortico {
namespace core {

class EventBus : public interfaces::IPublisher, public interfaces::ISubscriber {
public:
    EventBus();
    ~EventBus();

    /// Crée la file d'entrée et le mutex de la table d'abonnements. Ne
    /// démarre pas encore la tâche — voir start(). Non bloquant.
    bool init();

    /// Crée EventBusTask (xTaskCreatePinnedToCore, Core 1, priorité
    /// EVENT_BUS_TASK_PRIORITY — task_config.h). À appeler une seule fois,
    /// après que tous les abonnements d'initialisation ont été faits si
    /// possible (Document 07 §5, réduit la contention sur eventBusSubMutex).
    bool start();

    // ------------------------------------------------------------------
    // IPublisher
    // ------------------------------------------------------------------

    /// Empile l'événement dans qEventBusIn (xQueueSend, timeout 0 — ne
    /// bloque JAMAIS le publieur). Retourne faux si la file est pleine :
    /// l'appelant doit alors décider (drop, retry, log) — EventBus
    /// lui-même ne journalise pas ici (Logger n'est pas une dépendance de
    /// ce composant, pour éviter tout couplage circulaire L0<->L0).
     constants::PublishResult publish(const types::Event& event) override;

    // ------------------------------------------------------------------
    // ISubscriber
    // ------------------------------------------------------------------

    /// Enregistre listener pour le topic donné. Retourne faux si
    /// EVENT_BUS_MAX_TOPICS ou EVENT_BUS_MAX_LISTENERS_PER_TOPIC est atteint
    /// (limite statique, Document 03 §5) — à agrandir dans task_config.h si
    /// le besoin réel dépasse ces bornes, jamais par allocation dynamique.
   void subscribe(constants::EventID id, interfaces::IEventListener* listener) override;
    void unsubscribe(constants::EventID id, interfaces::IEventListener* listener) override;

    /// Résout un EventID en topic canonique (ex. EventID::FUEL_ANOMALY ->
    /// "fuel_anomaly"). Les managers doivent utiliser cette fonction plutôt
    /// que de saisir des chaînes à la main, pour éviter toute désynchro
    /// entre publish() (par EventID) et subscribe() (par topic string).
    static const char* eventIdToTopic(constants::EventID id);

    /// [Document 11 — support de test natif] Dépile et dispatche AU PLUS UN
    /// événement de qEventBusIn, sans bloquer indéfiniment. Retourne faux si
    /// la file était vide. Réservé aux tests unitaires natifs — le code de
    /// production utilise exclusivement start()/run() via EventBusTask.
    bool pumpOnce();

private:
    struct TopicSubscription {
        interfaces::IEventListener* listeners[tasks::EVENT_BUS_MAX_LISTENERS_PER_TOPIC];
        uint8_t listenerCount;
        bool used;
    };

    static void taskEntry(void* param);
    void run();
    void dispatch(const types::Event& event);
  
    QueueHandle_t _inputQueue;
    SemaphoreHandle_t _subMutex;
    TaskHandle_t _taskHandle;
    TopicSubscription _subscriptions[static_cast<uint32_t>(
        constants::EventID::EVENT_ID_COUNT)];
    bool _initialized;
};

}  // namespace core
}  // namespace fortico