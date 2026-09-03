// core/event_bus/event_bus.h
#ifndef EVENT_BUS_H
#define EVENT_BUS_H

#include <Arduino.h>

namespace ForticoCore {

enum class EventType : uint8_t {
    RFID_AUTHORIZED,
    RFID_UNAUTHORIZED,
    RFID_BADGE_REMOVED,
    ENGINE_ON,
    ENGINE_OFF,
    ALARM_TRIGGERED,
    GPS_FIX_ACQUIRED,
    GPS_FIX_LOST,
    NETWORK_CONNECTED,
    NETWORK_LOST,
    LOW_BATTERY,
    LOW_FUEL,
    SOS_PRESSED,
    CMD_RECEIVED,
    SYSTEM_READY,
    EVENT_COUNT
};

struct EventData {
    char stringData[32];
    int32_t intData;
    bool boolData;
    unsigned long timestamp;
};

typedef void (*EventCallback)(EventType type, const EventData& data);

class EventBus {
public:
    static const uint8_t MAX_LISTENERS = 5;

    static bool subscribe(EventType type, EventCallback callback);
    static void publish(EventType type, const EventData& data);
    static void publish(EventType type);

private:
    struct Listener {
        EventCallback callback;
        bool active;
    };

    static Listener _listeners[(uint8_t)EventType::EVENT_COUNT][MAX_LISTENERS];
};

} // namespace ForticoCore

#endif // EVENT_BUS_H