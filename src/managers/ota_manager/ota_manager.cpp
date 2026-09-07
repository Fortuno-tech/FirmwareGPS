#include "managers/ota_manager/ota_manager.h"

#ifdef ESP32_TARGET
#include <Update.h>
#endif

namespace fortico {
namespace managers {

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

OtaManager::OtaManager(IOtaBackend& backend)
	: _backend(backend), _initialized(false), _inProgress(false),
	  _expectedSize(0U), _bytesWritten(0U) {}
void OtaManager::init() { _initialized = true; reset(); }
void OtaManager::update() {}
void OtaManager::process() {}
void OtaManager::reset()
{
	if (_inProgress) _backend.abort();
	_inProgress = false; _expectedSize = 0U; _bytesWritten = 0U;
}
bool OtaManager::begin(std::size_t imageSize)
{
	if (!_initialized || _inProgress || imageSize == 0U) return false;
	if (!_backend.begin(imageSize)) return false;
	_expectedSize = imageSize; _bytesWritten = 0U; _inProgress = true;
	return true;
}
bool OtaManager::write(const uint8_t* data, std::size_t length)
{
	if (!_inProgress || data == nullptr || length == 0U ||
		_bytesWritten + length > _expectedSize || !_backend.write(data, length)) {
		return false;
	}
	_bytesWritten += length;
	return true;
}
bool OtaManager::finish()
{
	if (!_inProgress || _bytesWritten != _expectedSize || !_backend.finish()) return false;
	_inProgress = false;
	return true;
}
bool OtaManager::inProgress() const { return _inProgress; }
std::size_t OtaManager::bytesWritten() const { return _bytesWritten; }

}  // namespace managers
}  // namespace fortico
