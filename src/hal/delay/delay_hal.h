#pragma once

#include <cstdint>

namespace fortico {
namespace hal {

/// Attente coopérative : l'implémentation ESP32 rend la main à FreeRTOS.
class DelayHal {
public:
    void waitMs(uint32_t durationMs) const;
};

}  // namespace hal
}  // namespace fortico
