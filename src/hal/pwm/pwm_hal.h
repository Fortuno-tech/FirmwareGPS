#pragma once

#include <cstdint>

namespace fortico {
namespace hal {

class PwmHal {
public:
    bool init(uint8_t pin, uint32_t frequencyHz, uint8_t resolutionBits);
    void setDuty(uint8_t pin, uint32_t duty);
    void stop(uint8_t pin);

private:
    uint8_t pin_ = 0U;
    uint8_t channel_ = 0U;
    bool initialized_ = false;
    static uint8_t nextChannel_;
};

}  // namespace hal
}  // namespace fortico
