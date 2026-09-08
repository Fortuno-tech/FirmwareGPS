#pragma once

#include <cstddef>
#include <cstdint>

#include "interfaces/invs_driver.h"
#include "services/security/security_service.h"
#include "types/mqtt_types.h"

namespace fortico {
namespace services {

class ProvisioningService {
public:
    static constexpr std::size_t RootKeySize = 32U;
    static constexpr std::size_t OtaPubKeySize = 32U;

    ProvisioningService(SecurityService& securityService, interfaces::INvsDriver& nvsDriver);

    bool isProvisioned() const;
    bool provisionDevice(const uint8_t* rootKey, std::size_t rootKeyLen,
                         const uint8_t* otaPubKey, std::size_t otaPubKeyLen,
                         const types::MqttCredentials& mqttCreds,
                         const char* deviceId);
    bool storeOtaPublicKey(const uint8_t* key, std::size_t length);
    bool loadOtaPublicKey(uint8_t* key, std::size_t length) const;

    static bool hexToBytes(const char* hexStr, uint8_t* outBuf, std::size_t outLen);

private:
    SecurityService& _securityService;
    interfaces::INvsDriver& _nvsDriver;
};

}  // namespace services
}  // namespace fortico

