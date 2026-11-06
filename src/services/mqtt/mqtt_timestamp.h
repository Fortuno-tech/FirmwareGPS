#pragma once

#include <cstddef>

#include "drivers/rtc/rtc_driver.h"

namespace fortico {
namespace services {

class MqttTimestamp final {
public:
    static bool format(const drivers::RtcDateTime& dateTime,
                      char* output, std::size_t outputSize);
};

}  // namespace services
}  // namespace fortico
