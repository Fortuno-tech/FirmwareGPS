#pragma once

#include <cstdint>

#ifdef ESP32_TARGET
#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#endif

namespace fortico {
namespace hal {

#ifdef ESP32_TARGET
using TimerHandle = TimerHandle_t;
using TimerCallback = TimerCallbackFunction_t;
#else
using TimerHandle = void*;
using TimerCallback = void (*)(TimerHandle);
#endif

class TimerHal {
public:
    TimerHandle create(uint32_t periodMs, TimerCallback callback, bool autoReload);
    void start(TimerHandle timer);
    void stop(TimerHandle timer);
    void destroy(TimerHandle timer);
};

}  // namespace hal
}  // namespace fortico
