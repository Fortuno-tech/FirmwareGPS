#include "hal/nvs/nvs_hal.h"

#ifdef ESP32_TARGET
#include <Preferences.h>
#endif

namespace fortico {
namespace hal {

bool NvsHal::set(const char* nameSpace, const char* key, const uint8_t* data, size_t length) {
#ifdef ESP32_TARGET
    if (nameSpace == nullptr || key == nullptr || data == nullptr || length == 0U) {
        return false;
    }
    Preferences preferences;
    if (!preferences.begin(nameSpace, false)) {
        return false;
    }
    const bool written = preferences.putBytes(key, data, length) == length;
    preferences.end();
    return written;
#else
    (void)nameSpace;
    (void)key;
    (void)data;
    (void)length;
    return false;
#endif
}

bool NvsHal::read(const char* nameSpace, const char* key, uint8_t* buffer, size_t* length) {
#ifdef ESP32_TARGET
    if (nameSpace == nullptr || key == nullptr || buffer == nullptr || length == nullptr || *length == 0U) {
        return false;
    }
    Preferences preferences;
    if (!preferences.begin(nameSpace, true) || !preferences.isKey(key)) {
        preferences.end();
        return false;
    }
    const size_t storedLength = preferences.getBytesLength(key);
    if (storedLength > *length) {
        *length = storedLength;
        preferences.end();
        return false;
    }
    const bool read = preferences.getBytes(key, buffer, storedLength) == storedLength;
    *length = storedLength;
    preferences.end();
    return read;
#else
    (void)nameSpace;
    (void)key;
    (void)buffer;
    if (length != nullptr) {
        *length = 0U;
    }
    return false;
#endif
}

bool NvsHal::erase(const char* nameSpace, const char* key) {
#ifdef ESP32_TARGET
    if (nameSpace == nullptr || key == nullptr) {
        return false;
    }
    Preferences preferences;
    if (!preferences.begin(nameSpace, false)) {
        return false;
    }
    const bool erased = preferences.remove(key);
    preferences.end();
    return erased;
#else
    (void)nameSpace;
    (void)key;
    return false;
#endif
}
bool NvsHal::clear(const char* nameSpace)
{
#ifdef ESP32_TARGET

    if (nameSpace == nullptr || nameSpace[0] == '\0') {
        return false;
    }

    Preferences preferences;

    if (!preferences.begin(nameSpace, false)) {
        return false;
    }

    const bool cleared = preferences.clear();

    preferences.end();

    return cleared;

#else

    (void)nameSpace;
    return false;

#endif
}

}  // namespace hal
}  // namespace fortico
