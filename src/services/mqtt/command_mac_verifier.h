#pragma once

#include <cstddef>
#include <cstdint>

namespace fortico {
namespace services {

class ICommandMacVerifier {
public:
    virtual ~ICommandMacVerifier() = default;
    virtual bool verifyCommandMac(const uint8_t* message,
                                  std::size_t messageLength,
                                  const uint8_t* mac,
                                  std::size_t macLength) const = 0;
};

}  // namespace services
}  // namespace fortico
