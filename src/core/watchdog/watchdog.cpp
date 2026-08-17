/**
 * @file    watchdog.cpp
 * @brief   Implémentation de Watchdog — Fortico.
 */
#include "core/watchdog/watchdog.h"

#include <cstring>

#include "utils/timer_utils.h"

namespace fortico {
namespace core {

Watchdog::Watchdog()
    : _backend(nullptr),
      _timeoutMs(0),
      _registeredCount(0),
      _initialized(false) {
    for (auto& t : _tracked) {
        t.used = false;
        t.handle = nullptr;
        t.lastFeedMs = 0;
        t.name[0] = '\0';
    }
}

bool Watchdog::init(IWatchdogBackend* backend, uint32_t timeoutMs) {
    if (backend == nullptr || timeoutMs == 0) {
        return false;
    }

    if (!backend->init(timeoutMs)) {
        return false;
    }

    _backend = backend;
    _timeoutMs = timeoutMs;
    _initialized = true;
    return true;
}

bool Watchdog::registerTask(TaskHandle_t task, const char* name) {
    if (!_initialized || task == nullptr) {
        return false;
    }

    if (find(task) != nullptr) {
        return false;  // deja enregistree
    }

    if (!_backend->addTask(task)) {
        return false;
    }

    for (auto& t : _tracked) {
        if (!t.used) {
            t.used = true;
            t.handle = task;
            std::strncpy(t.name, name != nullptr ? name : "?", sizeof(t.name) - 1);
            t.name[sizeof(t.name) - 1] = '\0';
            t.lastFeedMs = 0;
            _registeredCount++;
            return true;
        }
    }

    // Table de bookkeeping pleine (MAX_TRACKED_TASKS atteint) : le backend
    // matériel a quand même enregistré la tâche (protection réelle assurée),
    // seul le suivi diagnostic est indisponible pour celle-ci.
    return false;
}

bool Watchdog::unregisterTask(TaskHandle_t task) {
    if (!_initialized || task == nullptr) {
        return false;
    }

    TrackedTask* t = find(task);
    if (t == nullptr) {
        return false;
    }

    _backend->removeTask(task);
    t->used = false;
    t->handle = nullptr;
    _registeredCount--;
    return true;
}

void Watchdog::feed(TaskHandle_t task) {
    if (!_initialized) {
        return;
    }

    // [RÉSOLU 13/07/2026] utils::Timer implémenté — source de temps unique
    // dans tout le firmware, plutôt que xTaskGetTickCount() recalculé ici.
    uint32_t nowMs = utils::Timer::millis();

    TrackedTask* t = find(task);
    if (t != nullptr) {
        t->lastFeedMs = nowMs;
    }

    // Réinitialisation matérielle réelle — DOIT être appelée depuis la
    // tâche elle-même (voir avertissement watchdog.h).
    _backend->resetSelf();
}

bool Watchdog::isTaskNearingTimeout(TaskHandle_t task, uint32_t marginMs, uint32_t nowMs) const {
    const TrackedTask* t = find(task);
    if (t == nullptr) {
        return false;  // tache non suivie : pas d'avis a donner
    }

    if (marginMs >= _timeoutMs) {
        return true;  // configuration incoherente : toujours "proche" par securite
    }

    uint32_t elapsed = nowMs - t->lastFeedMs;  // wraparound-safe (arithmetique non signee)
    return elapsed >= (_timeoutMs - marginMs);
}

Watchdog::TrackedTask* Watchdog::find(TaskHandle_t task) {
    for (auto& t : _tracked) {
        if (t.used && t.handle == task) {
            return &t;
        }
    }
    return nullptr;
}

const Watchdog::TrackedTask* Watchdog::find(TaskHandle_t task) const {
    for (const auto& t : _tracked) {
        if (t.used && t.handle == task) {
            return &t;
        }
    }
    return nullptr;
}

}  // namespace core
}  // namespace fortico