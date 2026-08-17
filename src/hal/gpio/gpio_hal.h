#pragma once

#include <cstdint>

namespace fortico {
namespace hal {

enum class GpioMode : uint8_t {
    Input,
    InputPullup,
    Output,
};

enum class GpioIntrType : uint8_t {
    Rising,
    Falling,
    Change,
    Low,
    High,
};

using GpioIsr = void (*)();

class GpioHal {
public:
    bool init(uint8_t pin, GpioMode mode);
    void write(uint8_t pin, bool value);
    bool read(uint8_t pin) const;
    bool attachInterrupt(uint8_t pin, GpioIsr isr, GpioIntrType type);
};

}  // namespace hal
}  // namespace fortico
