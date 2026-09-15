#pragma once

#include <cstdint>

namespace fortico {
namespace hal {

class RtcHal {
public:
    bool init();
    uint64_t getMonotonicUs() const;
};

}  // namespace hal
}  // namespace fortico
