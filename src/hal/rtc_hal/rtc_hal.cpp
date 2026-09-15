#include "hal/rtc_hal/rtc_hal.h"

#ifdef ESP32_TARGET
#include <esp_timer.h>
#else
#include <chrono>
#endif

namespace fortico {
namespace hal {

bool RtcHal::init() {
    return true;
}

uint64_t RtcHal::getMonotonicUs() const {
#ifdef ESP32_TARGET
    return static_cast<uint64_t>(esp_timer_get_time());
#else
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(now).count());
#endif
}

}  // namespace hal
}  // namespace fortico
