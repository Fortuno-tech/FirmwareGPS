/**
 * @file    event_bus.cpp
 * @brief   Implémentation d'EventBus — Fortico.
 */
#include "core/event_bus/event_bus.h"

namespace fortico {
namespace core {

EventBus::EventBus()
    : _inputQueue(nullptr),
      _subMutex(nullptr),
      _taskHandle(nullptr),
      _initialized(false) {
    for (auto& sub : _subscriptions) {
        sub.used = false;
        sub.listenerCount = 0;
    
    }
}

EventBus::~EventBus() {
    // [ASSUMPTION] En production, EventBus est un singleton créé une fois au
    // boot et jamais détruit avant coupure d'alimentation — ce destructeur
    // existe pour la propreté RAII et les tests natifs (évite les faux
    // positifs ASan/LeakSanitizer en environnement de test), pas parce
    // qu'une destruction en cours de fonctionnement est un scénario réel.
    if (_inputQueue != nullptr) {
        vQueueDelete(_inputQueue);
    }
}

bool EventBus::init() {
    _inputQueue = xQueueCreate(tasks::QUEUE_LEN_EVENT_BUS_IN, sizeof(types::Event));
    if (_inputQueue == nullptr) {
        return false;
    }

    _subMutex = xSemaphoreCreateMutex();
    if (_subMutex == nullptr) {
        return false;
    }

    _initialized = true;
    return true;
}

bool EventBus::start() {
    if (!_initialized) {
        return false;
    }

    BaseType_t result = xTaskCreatePinnedToCore(
        &EventBus::taskEntry,
        "EventBusTask",
        tasks::EVENT_BUS_TASK_STACK_WORDS,
        this,
        tasks::EVENT_BUS_TASK_PRIORITY,
        &_taskHandle,
        tasks::CORE_APP_CRITICAL);

    return result == pdTRUE;
}

constants::PublishResult EventBus::publish(const types::Event& event) {
    if (!_initialized) {
        return constants::PublishResult::NOT_INITIALIZED;
    }
    // Timeout 0 : ne bloque jamais le publieur (Document 07 §4bis.1, R-01).
    return xQueueSend(_inputQueue, &event, 0) == pdTRUE ? constants::PublishResult::OK : constants::PublishResult::QUEUE_FULL;
}
void EventBus::subscribe(constants::EventID id, interfaces::IEventListener* listener) {
    // ✅ Validation : listener valide et id dans les bornes
    if (listener == nullptr) {
        return;
    }
    const auto idx = static_cast<uint32_t>(id);
    if (idx >= static_cast<uint32_t>(constants::EventID::EVENT_ID_COUNT)) {
        return;
    }

    xSemaphoreTake(_subMutex, portMAX_DELAY);

    TopicSubscription& sub = _subscriptions[idx];
    if (sub.listenerCount < tasks::EVENT_BUS_MAX_LISTENERS_PER_TOPIC) {
        sub.listeners[sub.listenerCount] = listener;
        sub.listenerCount++;
        sub.used = true;
    }
    // Sinon : abonnement silencieusement refusé (table pleine)

    xSemaphoreGive(_subMutex);
}

void EventBus::unsubscribe(constants::EventID id, interfaces::IEventListener* listener) {
    if (listener == nullptr) {
        return;
    }
    const auto idx = static_cast<uint32_t>(id);
    if (idx >= static_cast<uint32_t>(constants::EventID::EVENT_ID_COUNT)) {
        return;
    }

    xSemaphoreTake(_subMutex, portMAX_DELAY);

    TopicSubscription& sub = _subscriptions[idx];
    for (uint8_t i = 0; i < sub.listenerCount; ++i) {
        if (sub.listeners[i] == listener) {
            // Compacte le tableau
            for (uint8_t j = i; j + 1 < sub.listenerCount; ++j) {
                sub.listeners[j] = sub.listeners[j + 1];
            }
            sub.listenerCount--;
            break;
        }
    }

    xSemaphoreGive(_subMutex);
}
void EventBus::taskEntry(void* param) {
    static_cast<EventBus*>(param)->run();
}

bool EventBus::pumpOnce() {
    types::Event event{};
    if (xQueueReceive(_inputQueue, &event, 0) == pdTRUE) {
        dispatch(event);
        return true;
    }
    return false;
}

void EventBus::run() {
    for (;;) {
        types::Event event{};
        // Bloque indefiniment jusqu'a reception - EventBusTask ne fait
        // rien d'autre (Document 07 SS4bis).
        if (xQueueReceive(_inputQueue, &event, portMAX_DELAY) == pdTRUE) {
            dispatch(event);
        }
        // TODO(Sprint 1, Watchdog) : feed watchdog ici une fois Watchdog
        // implémenté, si EventBusTask est ajoutée à la liste enregistrée
        // (Document 07 SS6 ne l'y liste pas actuellement - a confirmer).
    }
}

void EventBus::dispatch(const types::Event& event) {
    const auto idx = static_cast<uint32_t>(event.id);
    if (idx >= static_cast<uint32_t>(constants::EventID::EVENT_ID_COUNT)) {
        return;
    }

    xSemaphoreTake(_subMutex, portMAX_DELAY);


    TopicSubscription& sub = _subscriptions[idx];
    if (sub.used) {
        for (uint8_t i = 0; i < sub.listenerCount; ++i) {
            sub.listeners[i]->onEvent(event);
        }
    }

    xSemaphoreGive(_subMutex);
}

const char* EventBus::eventIdToTopic(constants::EventID id) {
    using constants::EventID;
    switch (id) {
        case EventID::RFID_CARD_READ:          return "rfid_card_read";
        case EventID::AUTH_AUTHORIZED:         return "auth_authorized";
        case EventID::AUTH_DENIED:             return "auth_denied";
        case EventID::GPS_FIX_ACQUIRED:        return "gps_fix_acquired";
        case EventID::GPS_LOST:                return "gps_lost";
        case EventID::NETWORK_CONNECTED:       return "network_connected";
        case EventID::NETWORK_LOST:            return "network_lost";
        case EventID::MQTT_CONNECTED:          return "mqtt_connected";
        case EventID::MQTT_DISCONNECTED:       return "mqtt_disconnected";
        case EventID::RELAY_ENGAGED:           return "relay_engaged";
        case EventID::RELAY_CUT:               return "relay_cut";
        case EventID::RELAY_EMERGENCY_CUT:     return "relay_emergency_cut";
        case EventID::RELAY_FAULT:             return "relay_fault";
        case EventID::SOS_BUTTON_PRESSED:      return "sos_button_pressed";
        case EventID::FUEL_ANOMALY:            return "fuel_anomaly";
        case EventID::BATTERY_LOW:             return "battery_low";
        case EventID::DIAGNOSTIC_WARNING:      return "diagnostic_warning";
        case EventID::OTA_AVAILABLE:           return "ota_available";
        case EventID::OTA_READY:               return "ota_ready";
        case EventID::OTA_STARTED:             return "ota_started";
        case EventID::OTA_FAILED:              return "ota_failed";
        case EventID::OTA_SUCCEEDED:           return "ota_succeeded";
        case EventID::SYSTEM_SELFTEST_PASSED:  return "system_selftest_passed";
        case EventID::SYSTEM_FAULT:            return "system_fault";
        default:                               return "unknown";
    }
}

}  // namespace core
}  // namespace fortico