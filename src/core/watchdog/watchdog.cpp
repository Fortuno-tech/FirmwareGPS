/**
 * @file    watchdog.cpp
 * @brief   Implémentation de Watchdog — Fortico.
 */
#include "core/watchdog/watchdog.h"

#include <cstring>

#include "utils/timer_utils.h"

namespace fortico {
namespace core {

bool EspTaskWdtBackend::init(uint32_t timeoutMs)
{
#ifdef ESP32_TARGET
    return esp_task_wdt_init(timeoutMs / 1000U, true) == ESP_OK;
#else
    (void)timeoutMs;
    return false;
#endif
}

bool EspTaskWdtBackend::addTask(TaskHandle_t task)
{
#ifdef ESP32_TARGET
    return task != nullptr && esp_task_wdt_add(task) == ESP_OK;
#else
    (void)task;
    return false;
#endif
}

bool EspTaskWdtBackend::removeTask(TaskHandle_t task)
{
#ifdef ESP32_TARGET
    return task != nullptr && esp_task_wdt_delete(task) == ESP_OK;
#else
    (void)task;
    return false;
#endif
}

bool EspTaskWdtBackend::resetSelf()
{
#ifdef ESP32_TARGET
    return esp_task_wdt_reset() == ESP_OK;
#else
    return false;
#endif
}

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

    TrackedTask* freeSlot = nullptr;
    for (auto& tracked : _tracked) {
        if (!tracked.used) {
            freeSlot = &tracked;
            break;
        }
    }
    if (freeSlot == nullptr) {
        return false;
    }

    if (!_backend->addTask(task)) {
        return false;
    }

    freeSlot->used = true;
    freeSlot->handle = task;
    std::strncpy(freeSlot->name, name != nullptr ? name : "?",
                 sizeof(freeSlot->name) - 1);
    freeSlot->name[sizeof(freeSlot->name) - 1] = '\0';
    freeSlot->lastFeedMs = utils::Timer::millis();
    _registeredCount++;
    return true;
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