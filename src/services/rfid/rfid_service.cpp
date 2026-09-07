#include "services/rfid/rfid_service.h"

#include <cstdio>
#include <cstring>

#include "constants/events.h"
#include "utils/timer_utils.h"

namespace fortico {
namespace services {

RfidService::RfidService(interfaces::IRfidDriver& driver,
                         AuthenticationService& authentication,
                         interfaces::IPublisher& publisher)
    : _driver(driver), _authentication(authentication), _publisher(publisher),
      _running(false), _lastUid{}, _lastUidLength(0U), _cardPresent(false)
{
}

bool RfidService::start()
{
    if (_driver.init() != constants::ErrorCode::OK &&
        !_driver.isInitialized()) {
        return false;
    }
    if (!_authentication.isRunning() && !_authentication.start()) {
        return false;
    }
    _running = _driver.start();
    return _running;
}

void RfidService::stop()
{
    _driver.stop();
    _running = false;
    clearUid();
}

bool RfidService::isRunning() const { return _running; }

void RfidService::update()
{
    if (!_running) return;
    _driver.poll();

    uint8_t uid[16]{};
    uint8_t uidLength = 0U;
    if (!_driver.readUid(uid, uidLength, 50U)) {
        clearUid();
        return;
    }
    if (uidLength == 0U || uidLength > sizeof(uid) ||
        (_cardPresent && isSameUid(uid, uidLength))) {
        return;
    }

    char formattedUid[sizeof(_lastUid) * 3U]{};
    formatUid(uid, uidLength, formattedUid, sizeof(formattedUid));
    const uint64_t timestamp = utils::Timer::millis();
    _publisher.publish({constants::EventID::RFID_CARD_READ, timestamp, 0U});
    _authentication.authenticate(formattedUid, timestamp);
    rememberUid(uid, uidLength);
}

bool RfidService::isSameUid(const uint8_t* uid, uint8_t length) const
{
    return length == _lastUidLength &&
           std::memcmp(uid, _lastUid, length) == 0;
}

void RfidService::rememberUid(const uint8_t* uid, uint8_t length)
{
    std::memcpy(_lastUid, uid, length);
    _lastUidLength = length;
    _cardPresent = true;
}

void RfidService::clearUid()
{
    std::memset(_lastUid, 0, sizeof(_lastUid));
    _lastUidLength = 0U;
    _cardPresent = false;
}

void RfidService::formatUid(const uint8_t* uid, uint8_t length,
                            char* output, uint8_t outputSize)
{
    if (output == nullptr || outputSize == 0U) return;
    uint8_t offset = 0U;
    for (uint8_t index = 0U; index < length; ++index) {
        const int written = std::snprintf(
            output + offset, outputSize - offset, "%s%02X",
            index == 0U ? "" : ":", uid[index]);
        if (written < 0 || static_cast<uint8_t>(written) >= outputSize - offset) {
            output[outputSize - 1U] = '\0';
            return;
        }
        offset = static_cast<uint8_t>(offset + written);
    }
}

}  // namespace services
}  // namespace fortico