#pragma once

#include <cstdint>

namespace fortico {
namespace hal {

class AdcHal {
public:
    bool init(uint8_t pin, uint8_t attenuationDb);
    uint16_t readRaw() const;
    uint32_t readMv() const;
    float readAvg(uint8_t samples) const;

private:
    uint8_t pin_ = 0U;
    bool initialized_ = false;
};

}  // namespace hal
}  // namespace fortico
