#pragma once

#include <cstdint>

#include "interfaces/idriver.h"

namespace fortico {
namespace interfaces {

class IRfidDriver : public IDriver {
public:
    ~IRfidDriver() override = default;

    virtual bool readUid(uint8_t* uid, uint8_t& uidLength,
                         uint32_t timeoutMs) = 0;
};

}  // namespace interfaces
}  // namespace fortico