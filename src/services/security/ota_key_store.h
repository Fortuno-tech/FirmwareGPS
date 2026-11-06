#pragma once

#include <cstddef>
#include <cstdint>

#include "interfaces/invs_driver.h"

namespace fortico {
namespace services {

class OtaKeyStore {
public:
    static constexpr std::size_t KeySize = 32U;

    explicit OtaKeyStore(interfaces::INvsDriver& nvsDriver);

    bool loadPublicKey(uint8_t key[KeySize]) const;
    bool storePublicKey(const uint8_t key[KeySize]);
    bool hasPublicKey() const;

private:
    interfaces::INvsDriver& _nvsDriver;
};

}  // namespace services
}  // namespace fortico

