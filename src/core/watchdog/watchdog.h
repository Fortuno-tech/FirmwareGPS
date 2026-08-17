/**
 * @file    watchdog.h
 * @brief   Watchdog matériel (wrapper esp_task_wdt) + suivi diagnostic —
 *          Fortico.
 *
 * @details Conformité R-02 (SOP §3.1) : le chien de garde matériel doit être
 *          activé et réinitialisé périodiquement ; reset automatique sous
 *          8 s en cas de blocage (WDT_TIMEOUT_S, hardware_config.h).
 *
 * @note    IWatchdogBackend abstrait esp_task_wdt (ESP-IDF, exposé tel quel
 *          sous Arduino-ESP32 — Document 03 §5) pour permettre un test natif
 *          de la logique de bookkeeping (Document 11) sans matériel réel.
 *          L'implémentation de production (EspTaskWdtBackend) reste à
 *          écrire séparément lors de l'intégration Arduino effective
 *          (Sprint 1/2) — hors périmètre de ce composant Core, qui ne doit
 *          pas dépendre directement d'esp_task_wdt.h (Document 04 §1,
 *          principe de couche appliqué ici aussi : Watchdog est L0 pur,
 *          l'implémentation ESP-IDF concrète du backend est un détail
 *          d'intégration plus bas niveau).
 *
 * @warning Watchdog::feed() DOIT être appelé depuis LA TÂCHE ELLE-MÊME
 *          (esp_task_wdt_reset() réinitialise pour la tâche appelante, pas
 *          pour un handle arbitraire passé en paramètre) — voir Document 07
 *          §6. Le paramètre task ci-dessous sert uniquement au bookkeeping
 *          diagnostic interne, pas à cibler la réinitialisation matérielle.
 */
#pragma once

#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace fortico {
namespace core {

/// Abstraction du mécanisme matériel réel (esp_task_wdt en production).
class IWatchdogBackend {
public:
    virtual ~IWatchdogBackend() = default;
    virtual bool init(uint32_t timeoutMs) = 0;
    virtual bool addTask(TaskHandle_t task) = 0;
    virtual bool removeTask(TaskHandle_t task) = 0;
    /// Réinitialise le watchdog pour LA TÂCHE APPELANTE (sémantique
   
    virtual bool resetSelf() = 0;
};

class Watchdog {
public:
    Watchdog();

    /// backend doit rester valide toute la durée de vie de Watchdog (pas de
    /// possession). timeoutMs typiquement hw::WDT_TIMEOUT_S * 1000.
    bool init(IWatchdogBackend* backend, uint32_t timeoutMs);

    /// Enregistre une tâche auprès du watchdog matériel ET du bookkeeping
    /// diagnostic interne (nom + horodatage du dernier feed).
    bool registerTask(TaskHandle_t task, const char* name);

    bool unregisterTask(TaskHandle_t task);

    /// À appeler depuis LA TÂCHE task elle-même, à chaque itération de sa
    /// boucle principale (Document 03 §5, Document 07 §6).
    void feed(TaskHandle_t task);

    uint32_t timeoutMs() const { return _timeoutMs; }

    uint8_t registeredTaskCount() const { return _registeredCount; }

    /// [Diagnostics, Document 06 §10] Vrai si `task` n'a pas nourri le
    /// watchdog depuis plus de (timeoutMs - marginMs) — permet à
    /// DiagnosticsManager de détecter une tâche en approche de reset AVANT
    /// que le reset matériel n'intervienne, pour investigation/log.
    bool isTaskNearingTimeout(TaskHandle_t task, uint32_t marginMs, uint32_t nowMs) const;

private:
    static constexpr uint8_t MAX_TRACKED_TASKS = 12;

    struct TrackedTask {
        TaskHandle_t handle;
        char name[16];
        uint32_t lastFeedMs;
        bool used;
    };

    TrackedTask* find(TaskHandle_t task);
    const TrackedTask* find(TaskHandle_t task) const;

    IWatchdogBackend* _backend;
    uint32_t _timeoutMs;
    uint8_t _registeredCount;
    bool _initialized;
    TrackedTask _tracked[MAX_TRACKED_TASKS];
};

}  // namespace core
}  // namespace fortico