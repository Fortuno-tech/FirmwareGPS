#pragma once

#include <cstddef>
#include <cstdint>

#ifdef ESP32_TARGET
class TwoWire;
#endif

namespace fortico {
namespace hal {

class I2cHal {
public:
    bool init(int sdaPin, int sclPin, uint32_t speedHz);
    bool write(uint8_t address, const uint8_t* buffer, size_t length, uint32_t timeoutMs);
    bool read(uint8_t address, uint8_t* buffer, size_t length, uint32_t timeoutMs);
    bool scan(uint8_t* foundAddresses, size_t maxAddresses, size_t* foundCount);

private:
#ifdef ESP32_TARGET
    ::TwoWire* wire_ = nullptr;
#endif
};

}  // namespace hal
}  // namespace fortico
