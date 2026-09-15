#include "services/security/ota_key_store.h"

#include "config/security_config.h"

namespace fortico {
namespace services {

OtaKeyStore::OtaKeyStore(interfaces::INvsDriver& nvsDriver)
    : _nvsDriver(nvsDriver) {}

bool OtaKeyStore::loadPublicKey(uint8_t key[KeySize]) const {
    if (key == nullptr || !_nvsDriver.isInitialized()) return false;
    if (!_nvsDriver.open(security::NVS_NAMESPACE, true)) return false;
    const bool loaded = _nvsDriver.getBlob(security::NVS_KEY_OTA_PUBKEY, key, KeySize);
    _nvsDriver.close();
    return loaded;
}

bool OtaKeyStore::storePublicKey(const uint8_t key[KeySize]) {
    if (key == nullptr || !_nvsDriver.isInitialized()) return false;
    if (!_nvsDriver.open(security::NVS_NAMESPACE, false)) return false;
    const bool stored = _nvsDriver.putBlob(security::NVS_KEY_OTA_PUBKEY, key, KeySize);
    _nvsDriver.close();
    return stored;
}

bool OtaKeyStore::hasPublicKey() const {
    uint8_t dummy[KeySize] = {};
    return loadPublicKey(dummy);
}

}  // namespace services
}  // namespace fortico

