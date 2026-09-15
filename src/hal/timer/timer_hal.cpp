#include "hal/timer/timer_hal.h"

namespace fortico {
namespace hal {

TimerHandle TimerHal::create(uint32_t periodMs, TimerCallback callback, bool autoReload) {
#ifdef ESP32_TARGET
    if (periodMs == 0U || callback == nullptr) {
        return nullptr;
    }
    return xTimerCreate(
        "ForticoTimer",
        pdMS_TO_TICKS(periodMs),
        autoReload ? pdTRUE : pdFALSE,
        nullptr,
        callback);
#else
    (void)periodMs;
    (void)callback;
    (void)autoReload;
    return nullptr;
#endif
}

void TimerHal::start(TimerHandle timer) {
#ifdef ESP32_TARGET
    if (timer != nullptr) {
        (void)xTimerStart(timer, 0U);
    }
#else
    (void)timer;
#endif
}

void TimerHal::stop(TimerHandle timer) {
#ifdef ESP32_TARGET
    if (timer != nullptr) {
        (void)xTimerStop(timer, 0U);
    }
#else
    (void)timer;
#endif
}

void TimerHal::destroy(TimerHandle timer) {
#ifdef ESP32_TARGET
    if (timer != nullptr) {
        (void)xTimerDelete(timer, 0U);
    }
#else
    (void)timer;
#endif
}

}  // namespace hal
}  // namespace fortico
