#include "services/mqtt/mqtt_context.h"

#include <cstring>
#include <Arduino.h>

#include "services/mqtt/mqtt_identity.h"
#include "services/mqtt/mqtt_timestamp.h"

namespace fortico {
namespace services {

bool MqttContext::build(MqttDeviceContext& context,
                        const drivers::RtcDriver& rtc)
{
    std::memset(&context, 0, sizeof(context));
    context.vehicleId = MqttIdentity::vehicleId();
    context.firmware = MqttIdentity::firmwareVersion();
    context.timestampValid = false;
    if (!MqttIdentity::deviceId(context.deviceId, sizeof(context.deviceId))) {
        return false;
    }
    drivers::RtcDateTime dateTime{};
    if (!const_cast<drivers::RtcDriver&>(rtc).readDateTime(dateTime)) {
		Serial.println("[WARN][RTC] timestamp UTC indisponible");
        return true;
    }
    context.timestampValid = MqttTimestamp::format(
        dateTime, context.timestamp, sizeof(context.timestamp));
    return true;
}

}  // namespace services
}  // namespace fortico
