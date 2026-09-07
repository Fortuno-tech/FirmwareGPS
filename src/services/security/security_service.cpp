#include "services/security/security_service.h"

#include <cstring>

#include "config/security_config.h"

#ifdef ESP32_TARGET
#include <mbedtls/gcm.h>
#include <mbedtls/md.h>
#endif

namespace fortico {
namespace services {

SecurityService::SecurityService(interfaces::INvsDriver& nvsDriver)
    : _nvsDriver(nvsDriver), _rootKey{}, _running(false), _keyLoaded(false) {}

bool SecurityService::start()
{
    if (!_nvsDriver.isInitialized()) return false;
    if (!_nvsDriver.open(security::NVS_NAMESPACE, false)) return false;
    _keyLoaded = _nvsDriver.getBlob(security::NVS_KEY_SECURITY_ROOT,
                                    _rootKey, KeySize);
    _nvsDriver.close();
    _running = _keyLoaded;
    return _running;
}

void SecurityService::stop()
{
    std::memset(_rootKey, 0, sizeof(_rootKey));
    _keyLoaded = false;
    _running = false;
}

bool SecurityService::isRunning() const { return _running && _keyLoaded; }

bool SecurityService::provisionRootKey(const uint8_t* key, std::size_t length)
{
    if (key == nullptr || length != KeySize || !_nvsDriver.isInitialized()) return false;
    if (!_nvsDriver.open(security::NVS_NAMESPACE, false)) return false;
    const bool stored = _nvsDriver.putBlob(security::NVS_KEY_SECURITY_ROOT, key, KeySize);
    _nvsDriver.close();
    if (stored) {
        std::memcpy(_rootKey, key, KeySize);
        _keyLoaded = true;
        _running = true;
    }
    return stored;
}

bool SecurityService::hashBadge(const char* normalizedUid, uint8_t* digest,
                                std::size_t digestSize) const
{
    if (!isRunning() || normalizedUid == nullptr || digest == nullptr ||
        digestSize < HmacSize) return false;
#ifdef ESP32_TARGET
    const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    return info != nullptr && mbedtls_md_hmac(info, _rootKey, KeySize,
        reinterpret_cast<const unsigned char*>(normalizedUid),
        std::strlen(normalizedUid), digest) == 0;
#else
    (void)normalizedUid; (void)digest; (void)digestSize;
    return false;
#endif
}

bool SecurityService::encrypt(const uint8_t* plaintext, std::size_t plaintextSize,
                              const uint8_t* nonce, std::size_t nonceSize,
                              uint8_t* ciphertext, std::size_t ciphertextSize,
                              uint8_t* tag, std::size_t tagSize) const
{
    if (!isRunning() || plaintext == nullptr || nonce == nullptr || ciphertext == nullptr ||
        tag == nullptr || nonceSize != GcmNonceSize || tagSize != GcmTagSize ||
        ciphertextSize < plaintextSize) return false;
#ifdef ESP32_TARGET
    mbedtls_gcm_context context;
    mbedtls_gcm_init(&context);
    const int result = mbedtls_gcm_setkey(&context, MBEDTLS_CIPHER_ID_AES,
                                          _rootKey, KeySize * 8U) == 0
        ? mbedtls_gcm_crypt_and_tag(&context, MBEDTLS_GCM_ENCRYPT,
            plaintextSize, nonce, nonceSize, nullptr, 0U, plaintext,
            ciphertext, tagSize, tag) : -1;
    mbedtls_gcm_free(&context);
    return result == 0;
#else
    (void)plaintext; (void)plaintextSize; (void)nonce; (void)nonceSize;
    (void)ciphertext; (void)ciphertextSize; (void)tag; (void)tagSize;
    return false;
#endif
}

bool SecurityService::decrypt(const uint8_t* ciphertext, std::size_t ciphertextSize,
                              const uint8_t* nonce, std::size_t nonceSize,
                              const uint8_t* tag, std::size_t tagSize,
                              uint8_t* plaintext, std::size_t plaintextSize) const
{
    if (!isRunning() || ciphertext == nullptr || nonce == nullptr || tag == nullptr ||
        plaintext == nullptr || nonceSize != GcmNonceSize || tagSize != GcmTagSize ||
        plaintextSize < ciphertextSize) return false;
#ifdef ESP32_TARGET
    mbedtls_gcm_context context;
    mbedtls_gcm_init(&context);
    const int result = mbedtls_gcm_setkey(&context, MBEDTLS_CIPHER_ID_AES,
                                          _rootKey, KeySize * 8U) == 0
        ? mbedtls_gcm_auth_decrypt(&context, ciphertextSize, nonce, nonceSize,
            nullptr, 0U, tag, tagSize, ciphertext, plaintext) : -1;
    mbedtls_gcm_free(&context);
    return result == 0;
#else
    (void)ciphertext; (void)ciphertextSize; (void)nonce; (void)nonceSize;
    (void)tag; (void)tagSize; (void)plaintext; (void)plaintextSize;
    return false;
#endif
}

}  // namespace services
}  // namespace fortico