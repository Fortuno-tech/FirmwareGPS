#include "hal/delay/delay_hal.h"

#ifdef ESP32_TARGET
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

namespace fortico {
namespace hal {

void DelayHal::waitMs(uint32_t durationMs) const
{
#ifdef ESP32_TARGET
    if (durationMs == 0U) {
        taskYIELD();
        return;
    }
    const TickType_t ticks = pdMS_TO_TICKS(durationMs);
    vTaskDelay(ticks == 0U ? 1U : ticks);
#else
    (void)durationMs;
#endif
}

}  // namespace hal
}  // namespace fortico
