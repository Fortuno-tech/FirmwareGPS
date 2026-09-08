#include "services/security/provisioning_service.h"

#include <cstring>
#include <cctype>

#include "config/security_config.h"

namespace fortico {
namespace services {

namespace {

uint8_t hexNibble(char c) {
    if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
    if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(c - 'A' + 10);
    return 0xFFU;
}

}  // namespace

ProvisioningService::ProvisioningService(SecurityService& securityService,
                                         interfaces::INvsDriver& nvsDriver)
    : _securityService(securityService), _nvsDriver(nvsDriver) {}

bool ProvisioningService::isProvisioned() const {
    if (!_nvsDriver.isInitialized()) return false;
    if (!_nvsDriver.open(security::NVS_NAMESPACE, true)) return false;
    uint8_t dummy[RootKeySize] = {};
    const bool hasRootKey = _nvsDriver.getBlob(security::NVS_KEY_SECURITY_ROOT, dummy, sizeof(dummy));
    _nvsDriver.close();
    return hasRootKey;
}

bool ProvisioningService::hexToBytes(const char* hexStr, uint8_t* outBuf, std::size_t outLen) {
    if (hexStr == nullptr || outBuf == nullptr || std::strlen(hexStr) != outLen * 2U) {
        return false;
    }
    for (std::size_t i = 0U; i < outLen; ++i) {
        const uint8_t high = hexNibble(hexStr[i * 2U]);
        const uint8_t low = hexNibble(hexStr[i * 2U + 1U]);
        if (high == 0xFFU || low == 0xFFU) {
            return false;
        }
        outBuf[i] = static_cast<uint8_t>((high << 4U) | low);
    }
    return true;
}

bool ProvisioningService::storeOtaPublicKey(const uint8_t* key, std::size_t length) {
    if (key == nullptr || length != OtaPubKeySize || !_nvsDriver.isInitialized()) return false;
    if (!_nvsDriver.open(security::NVS_NAMESPACE, false)) return false;
    const bool stored = _nvsDriver.putBlob(security::NVS_KEY_OTA_PUBKEY, key, length);
    _nvsDriver.close();
    return stored;
}

bool ProvisioningService::loadOtaPublicKey(uint8_t* key, std::size_t length) const {
    if (key == nullptr || length != OtaPubKeySize || !_nvsDriver.isInitialized()) return false;
    if (!_nvsDriver.open(security::NVS_NAMESPACE, true)) return false;
    const bool loaded = _nvsDriver.getBlob(security::NVS_KEY_OTA_PUBKEY, key, length);
    _nvsDriver.close();
    return loaded;
}

bool ProvisioningService::provisionDevice(const uint8_t* rootKey, std::size_t rootKeyLen,
                                           const uint8_t* otaPubKey, std::size_t otaPubKeyLen,
                                           const types::MqttCredentials& mqttCreds,
                                           const char* deviceId) {
    if (isProvisioned()) {
        return false; // Prevents double provisioning
    }
    if (!_securityService.provision(rootKey, rootKeyLen, mqttCreds, deviceId)) {
        return false;
    }
    if (!storeOtaPublicKey(otaPubKey, otaPubKeyLen)) {
        return false;
    }
    return true;
}

}  // namespace services
}  // namespace fortico

