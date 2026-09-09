#pragma once

#include <cstddef>
#include <cstdint>

#include "interfaces/invs_driver.h"
#include "services/security/security_service.h"
#include "types/mqtt_types.h"

namespace fortico {
namespace drivers {
class Sim7600Driver;
}

namespace services {

class ProvisioningService {
public:
    static constexpr std::size_t RootKeySize = 32U;
    static constexpr std::size_t OtaPubKeySize = 32U;
    static constexpr std::size_t ExpectedHiveMqCertSize = 1939U;
    static constexpr const char* HiveMqCertFileName = "hivemq-ca.pem";

    ProvisioningService(SecurityService& securityService,
                        interfaces::INvsDriver& nvsDriver,
                        drivers::Sim7600Driver* sim7600Driver = nullptr);

    void setSim7600Driver(drivers::Sim7600Driver* sim7600Driver);
    bool hasModemCertificate(const char* certificateName = HiveMqCertFileName) const;
    bool installModemCertificate(const char* certificateName,
                                 std::size_t expectedSize,
                                 const char* base64Data);

    bool isProvisioned() const;
    bool provisionDevice(const uint8_t* rootKey, std::size_t rootKeyLen,
                         const uint8_t* otaPubKey, std::size_t otaPubKeyLen,
                         const types::MqttCredentials& mqttCreds,
                         const char* deviceId);
    bool storeOtaPublicKey(const uint8_t* key, std::size_t length);
    bool loadOtaPublicKey(uint8_t* key, std::size_t length) const;

    static bool hexToBytes(const char* hexStr, uint8_t* outBuf, std::size_t outLen);
    static bool base64Decode(const char* input, std::size_t inputLen, uint8_t* output, std::size_t& outputLen);

private:
    SecurityService& _securityService;
    interfaces::INvsDriver& _nvsDriver;
    drivers::Sim7600Driver* _sim7600Driver;
};

}  // namespace services
}  // namespace fortico

