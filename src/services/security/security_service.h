#pragma once

#include <cstddef>
#include <cstdint>

#include "interfaces/invs_driver.h"
#include "interfaces/iservice.h"

namespace fortico {
namespace services {

class SecurityService : public interfaces::IService {
public:
    static constexpr std::size_t KeySize = 32U;
    static constexpr std::size_t HmacSize = 32U;
    static constexpr std::size_t GcmNonceSize = 12U;
    static constexpr std::size_t GcmTagSize = 16U;

    explicit SecurityService(interfaces::INvsDriver& nvsDriver);

    bool start() override;
    void stop() override;
    bool isRunning() const override;

    bool provisionRootKey(const uint8_t* key, std::size_t length);
    bool hashBadge(const char* normalizedUid, uint8_t* digest,
                   std::size_t digestSize) const;
    bool encrypt(const uint8_t* plaintext, std::size_t plaintextSize,
                 const uint8_t* nonce, std::size_t nonceSize,
                 uint8_t* ciphertext, std::size_t ciphertextSize,
                 uint8_t* tag, std::size_t tagSize) const;
    bool decrypt(const uint8_t* ciphertext, std::size_t ciphertextSize,
                 const uint8_t* nonce, std::size_t nonceSize,
                 const uint8_t* tag, std::size_t tagSize,
                 uint8_t* plaintext, std::size_t plaintextSize) const;

private:
    interfaces::INvsDriver& _nvsDriver;
    uint8_t _rootKey[KeySize];
    bool _running;
    bool _keyLoaded;
};

}  // namespace services
}  // namespace fortico