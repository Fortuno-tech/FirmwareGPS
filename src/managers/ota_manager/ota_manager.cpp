#include "managers/ota_manager/ota_manager.h"

#include <cstring>

#ifdef ESP32_TARGET
#include <Update.h>
#endif

namespace fortico {
namespace managers {

Ed25519OtaVerifier::Ed25519OtaVerifier(const uint8_t* publicKey)
	: _publicKey{}, _configured(publicKey != nullptr), _active(false)
{
	if (publicKey != nullptr) {
		std::memcpy(_publicKey, publicKey, PublicKeySize);
	}
}

bool Ed25519OtaVerifier::begin(std::size_t imageSize)
{
	(void)imageSize;
#ifdef ESP32_TARGET
	if (!_configured || sodium_init() < 0) return false;
	_active = crypto_sign_ed25519ph_init(&_state) == 0;
	return _active;
#else
	return false;
#endif
}

bool Ed25519OtaVerifier::update(const uint8_t* data, std::size_t length)
{
	if (!_active || data == nullptr || length == 0U) return false;
#ifdef ESP32_TARGET
	return crypto_sign_ed25519ph_update(&_state, data, length) == 0;
#else
	return false;
#endif
}

bool Ed25519OtaVerifier::verify(const uint8_t* signature, std::size_t length)
{
	if (!_active || signature == nullptr || length != SignatureSize) return false;
#ifdef ESP32_TARGET
	const bool valid = crypto_sign_ed25519ph_final_verify(
		&_state, signature, _publicKey) == 0;
	_active = false;
	return valid;
#else
	return false;
#endif
}

void Ed25519OtaVerifier::reset()
{
	_active = false;
}

bool EspOtaBackend::begin(std::size_t imageSize)
{
#ifdef ESP32_TARGET
	return Update.begin(imageSize);
#else
	(void)imageSize; return false;
#endif
}
bool EspOtaBackend::write(const uint8_t* data, std::size_t length)
{
#ifdef ESP32_TARGET
	    return data != nullptr &&
		    Update.write(const_cast<uint8_t*>(data), length) == length;
#else
	(void)data; (void)length; return false;
#endif
}
bool EspOtaBackend::finish()
{
#ifdef ESP32_TARGET
	return Update.end(true);
#else
	return false;
#endif
}
void EspOtaBackend::abort()
{
#ifdef ESP32_TARGET
	Update.abort();
#endif
}

OtaManager::OtaManager(IOtaBackend& backend, IOtaSignatureVerifier& verifier)
	: _backend(backend), _verifier(verifier), _initialized(false), _inProgress(false),
	  _expectedSize(0U), _bytesWritten(0U) {}
void OtaManager::init() { _initialized = true; reset(); }
void OtaManager::update() {}
void OtaManager::process() {}
void OtaManager::reset()
{
	if (_inProgress) _backend.abort();
	_verifier.reset();
	_inProgress = false; _expectedSize = 0U; _bytesWritten = 0U;
}
bool OtaManager::begin(std::size_t imageSize)
{
	if (!_initialized || _inProgress || imageSize == 0U) return false;
	if (!_verifier.begin(imageSize)) return false;
	if (!_backend.begin(imageSize)) {
		_verifier.reset();
		return false;
	}
	_expectedSize = imageSize; _bytesWritten = 0U; _inProgress = true;
	return true;
}
bool OtaManager::write(const uint8_t* data, std::size_t length)
{
	if (!_inProgress || data == nullptr || length == 0U ||
		_bytesWritten + length > _expectedSize || !_backend.write(data, length) ||
		!_verifier.update(data, length)) {
		if (_inProgress) reset();
		return false;
	}
	_bytesWritten += length;
	return true;
}
bool OtaManager::finish(const uint8_t* signature, std::size_t signatureLength)
{
	if (!_inProgress || _bytesWritten != _expectedSize ||
		!_verifier.verify(signature, signatureLength) || !_backend.finish()) {
		if (_inProgress) reset();
		return false;
	}
	_inProgress = false;
	return true;
}
bool OtaManager::inProgress() const { return _inProgress; }
std::size_t OtaManager::bytesWritten() const { return _bytesWritten; }

}  // namespace managers
}  // namespace fortico
