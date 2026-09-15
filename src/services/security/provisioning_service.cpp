#include "services/security/provisioning_service.h"

#include <cstring>
#include <cctype>

#include "config/security_config.h"

#ifdef ESP32_TARGET
#include <Arduino.h>
#include "drivers/sim7600/sim7600_driver.h"
#endif

namespace fortico {
namespace services {

namespace {

uint8_t hexNibble(char c) {
    if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
    if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return static_cast<uint8_t>(c - 'A' + 10);
    return 0xFFU;
}

int base64CharVal(char c) {
    if (c >= 'A' && c <= 'Z') return static_cast<int>(c - 'A');
    if (c >= 'a' && c <= 'z') return static_cast<int>(c - 'a' + 26);
    if (c >= '0' && c <= '9') return static_cast<int>(c - '0' + 52);
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

}  // namespace

ProvisioningService::ProvisioningService(SecurityService& securityService,
                                         interfaces::INvsDriver& nvsDriver,
                                         drivers::Sim7600Driver* sim7600Driver)
    : _securityService(securityService), _nvsDriver(nvsDriver), _sim7600Driver(sim7600Driver) {}

void ProvisioningService::setSim7600Driver(drivers::Sim7600Driver* sim7600Driver) {
    _sim7600Driver = sim7600Driver;
}

bool ProvisioningService::hasModemCertificate(const char* certificateName) const {
#ifdef ESP32_TARGET
    if (_sim7600Driver == nullptr || certificateName == nullptr) return false;
    return _sim7600Driver->hasCertificate(certificateName);
#else
    (void)certificateName;
    return _sim7600Driver != nullptr;
#endif
}

bool ProvisioningService::installModemCertificate(const char* certificateName,
                                                  std::size_t expectedSize,
                                                  const char* base64Data) {
    if (certificateName == nullptr || base64Data == nullptr) {
        return false;
    }
    if (expectedSize != ExpectedHiveMqCertSize) {
#ifdef ESP32_TARGET
        Serial.printf("[PROV] Size mismatch: %u vs %u\n",
                      static_cast<unsigned int>(expectedSize),
                      static_cast<unsigned int>(ExpectedHiveMqCertSize));
#endif
        return false;
    }

    const std::size_t base64Len = std::strlen(base64Data);
    uint8_t certBuffer[2048] = {};
    std::size_t decodedLen = sizeof(certBuffer);

    if (!base64Decode(base64Data, base64Len, certBuffer, decodedLen)) {
#ifdef ESP32_TARGET
        Serial.printf("[PROV] base64Decode failed, b64Len=%u\n",
                      static_cast<unsigned int>(base64Len));
#endif
        return false;
    }

    if (decodedLen != expectedSize || decodedLen != ExpectedHiveMqCertSize) {
#ifdef ESP32_TARGET
        Serial.printf("[PROV] Decoded len mismatch: %u vs %u\n",
                      static_cast<unsigned int>(decodedLen),
                      static_cast<unsigned int>(expectedSize));
#endif
        return false;
    }

#ifdef ESP32_TARGET
    if (_sim7600Driver == nullptr) {
        Serial.println(F("[PROV] Sim7600Driver is null!"));
        return false;
    }
    Serial.println(F("[PROV] Deleting old certificate from SIM7600..."));
    _sim7600Driver->deleteCertificate(certificateName);
    Serial.println(F("[PROV] Uploading certificate to SIM7600..."));
    const bool ok = _sim7600Driver->downloadCertificate(certificateName, certBuffer, decodedLen);
    if (!ok) {
        Serial.println(F("[PROV] downloadCertificate failed!"));
    }
    return ok;
#else
    return _sim7600Driver != nullptr;
#endif
}

bool ProvisioningService::base64Decode(const char* input, std::size_t inputLen,
                                       uint8_t* output, std::size_t& outputLen) {
    if (input == nullptr || output == nullptr) return false;

    // Trim trailing whitespace or newlines
    while (inputLen > 0U && (input[inputLen - 1U] == '\r' ||
                             input[inputLen - 1U] == '\n' ||
                             input[inputLen - 1U] == ' ' ||
                             input[inputLen - 1U] == '\t')) {
        --inputLen;
    }

    if (inputLen == 0U || (inputLen % 4U) != 0U) {
        return false;
    }

    std::size_t outIdx = 0U;
    const std::size_t maxOutput = outputLen;

    for (std::size_t i = 0U; i < inputLen; i += 4U) {
        const char c0 = input[i];
        const char c1 = input[i + 1U];
        const char c2 = input[i + 2U];
        const char c3 = input[i + 3U];

        if (c0 == '=' || c1 == '=') return false;

        const int v0 = base64CharVal(c0);
        const int v1 = base64CharVal(c1);
        if (v0 < 0 || v1 < 0) return false;

        const uint8_t b0 = static_cast<uint8_t>((v0 << 2) | (v1 >> 4));
        if (outIdx >= maxOutput) return false;
        output[outIdx++] = b0;

        if (c2 == '=') {
            if (c3 != '=' || (i + 4U) != inputLen) return false;
            break;
        }

        const int v2 = base64CharVal(c2);
        if (v2 < 0) return false;

        const uint8_t b1 = static_cast<uint8_t>(((v1 & 0x0F) << 4) | (v2 >> 2));
        if (outIdx >= maxOutput) return false;
        output[outIdx++] = b1;

        if (c3 == '=') {
            if ((i + 4U) != inputLen) return false;
            break;
        }

        const int v3 = base64CharVal(c3);
        if (v3 < 0) return false;

        const uint8_t b2 = static_cast<uint8_t>(((v2 & 0x03) << 6) | v3);
        if (outIdx >= maxOutput) return false;
        output[outIdx++] = b2;
    }

    outputLen = outIdx;
    return true;
}

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
                                           const char* deviceId,
                                           const char* vehicleId) {
    if (isProvisioned()) {
        return false; // Prevents double provisioning
    }
    if (!_securityService.provision(rootKey, rootKeyLen, mqttCreds, deviceId,
                                    vehicleId)) {
        return false;
    }
    if (!storeOtaPublicKey(otaPubKey, otaPubKeyLen)) {
        _securityService.clearProvisioning();
        return false;
    }
    return true;
}

}  // namespace services
}  // namespace fortico

