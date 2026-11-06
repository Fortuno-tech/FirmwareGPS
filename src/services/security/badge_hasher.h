#pragma once

#include <cstddef>
#include <cstdint>

namespace fortico {
namespace services {

class IBadgeHasher {
public:
    static constexpr std::size_t DigestSize = 32U;

    virtual ~IBadgeHasher() = default;
    virtual bool isRunning() const = 0;
    virtual bool hashBadge(const char* normalizedUid, uint8_t* digest,
                           std::size_t digestSize) const = 0;
};

}  // namespace services
}  // namespace fortico
