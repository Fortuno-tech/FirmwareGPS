#pragma once

#include <cstddef>

#include "drivers/rtc/rtc_driver.h"

namespace fortico {
namespace services {

struct MqttDeviceContext {
    char deviceId[16];
    const char* vehicleId;
    const char* firmware;
    char timestamp[24];
    bool timestampValid;
};

class MqttContext final {
public:
    static bool build(MqttDeviceContext& context,
                      const drivers::RtcDriver& rtc);
};

}  // namespace services
}  // namespace fortico
