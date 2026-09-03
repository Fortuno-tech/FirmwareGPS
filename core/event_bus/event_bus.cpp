// core/event_bus/event_bus.cpp
#include "event_bus.h"

namespace ForticoCore {

EventBus::Listener EventBus::_listeners[(uint8_t)EventType::EVENT_COUNT][MAX_LISTENERS] = {};

bool EventBus::subscribe(EventType type, EventCallback callback) {
    uint8_t typeIndex = (uint8_t)type;

    for (uint8_t i = 0; i < MAX_LISTENERS; i++) {
        if (!_listeners[typeIndex][i].active) {
            _listeners[typeIndex][i].callback = callback;
            _listeners[typeIndex][i].active = true;

            Serial.printf("[EVENT_BUS] Listener ajouté pour event %d\n",
                          typeIndex);
            return true;
        }
    }

    Serial.printf("[EVENT_BUS] ERREUR: Max listeners atteint pour event %d\n",
                  typeIndex);
    return false;
}

void EventBus::publish(EventType type, const EventData& data) {
    uint8_t typeIndex = (uint8_t)type;

    for (uint8_t i = 0; i < MAX_LISTENERS; i++) {
        if (_listeners[typeIndex][i].active &&
            _listeners[typeIndex][i].callback) {
            _listeners[typeIndex][i].callback(type, data);
        }
    }
}

void EventBus::publish(EventType type) {
    EventData emptyData;
    memset(&emptyData, 0, sizeof(emptyData));
    emptyData.timestamp = millis();
    publish(type, emptyData);
}

} // namespace ForticoCore