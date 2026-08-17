#pragma once

#include <cstddef>
#include <cstdint>

namespace fortico {
namespace hal {

class NvsHal {
public:
    bool set(const char* nameSpace, const char* key, const uint8_t* data, size_t length);
    bool read(const char* nameSpace, const char* key, uint8_t* buffer, size_t* length);
    bool erase(const char* nameSpace, const char* key);
    bool clear(const char* nameSpace);
};

}  // namespace hal
}  // namespace fortico
