#pragma once

#include <cstddef>
#include <cstdint>

namespace fortico {
namespace hal {

class FlashHal {
public:
    bool write(uint32_t address, const uint8_t* data, size_t length);
    bool read(uint32_t address, uint8_t* buffer, size_t length);
    bool erase(uint32_t sectorAddress);
};

}  // namespace hal
}  // namespace fortico
