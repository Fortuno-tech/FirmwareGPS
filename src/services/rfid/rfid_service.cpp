#include "services/rfid/rfid_service.h"

#include <Arduino.h>
#include <cstdio>
#include <cstring>

#include "config/hardware_config.h"
#include "config/logging_config.h"
#include "config/task_config.h"
#include "constants/events.h"
#include "utils/timer_utils.h"

namespace fortico {
namespace services {

RfidService::RfidService(
    interfaces::IRfidDriver& driver,
    AuthenticationService& authentication,
    interfaces::IPublisher& publisher,
    core::Logger& logger)
    : _driver(driver)
    , _authentication(authentication)
    , _publisher(publisher)
    , _logger(logger)
    , _running(false)
    , _lastUid{}
    , _lastUidLength(0U)
    , _cardPresent(false)
    , _nextPollMs(0U)
    , _updateCount(0U)
    , _uidCount(0U)
    , _readFailureCount(0U)
    , _resultQueue(nullptr)
    , _taskHandle(nullptr)
    , _taskRunning(false)
{
}

bool RfidService::start()
{
    _logger.info(logging::TAG_RFID, "Demarrage service RFID...");

    if (_driver.init() != constants::ErrorCode::OK) {
        _logger.error(logging::TAG_RFID, "Initialisation du driver RFID impossible");
        return false;
    }

    if (!_authentication.isRunning()) {
        if (!_authentication.start()) {
            _logger.error(logging::TAG_RFID, "Demarrage du service d'authentification impossible");
            return false;
        }
    }

    if (!_driver.start()) {
        _logger.error(logging::TAG_RFID, "Demarrage du driver RFID impossible");
        return false;
    }

    _resultQueue = xQueueCreate(tasks::QUEUE_LEN_RFID_RESULT, sizeof(Result));
    if (_resultQueue == nullptr) {
        _driver.stop();
        return false;
    }

    _taskRunning = true;
    if (xTaskCreatePinnedToCore(&RfidService::taskEntry, "RfidServiceTask",
                                tasks::RFID_TASK_STACK_WORDS, this,
                                tasks::RFID_TASK_PRIORITY, &_taskHandle,
                                tasks::CORE_APP_CRITICAL) != pdTRUE) {
        _taskRunning = false;
        vQueueDelete(_resultQueue);
        _resultQueue = nullptr;
        _driver.stop();
        return false;
    }

    _running = true;
    clearUid();

    _logger.info(logging::TAG_RFID, "Service RFID demarre avec succes");
    return true;
}

void RfidService::stop()
{
    _taskRunning = false;
    if (_taskHandle != nullptr) {
        vTaskDelete(_taskHandle);
        _taskHandle = nullptr;
    }
    _driver.stop();
    _running = false;

    clearUid();
    if (_resultQueue != nullptr) {
        vQueueDelete(_resultQueue);
        _resultQueue = nullptr;
    }

    _logger.info(logging::TAG_RFID, "Service RFID arrete");
}

bool RfidService::isRunning() const
{
    return _running && _driver.isHealthy();
}

void RfidService::update()
{
}

bool RfidService::receiveResult(Result& result)
{
    return _resultQueue != nullptr &&
        xQueueReceive(_resultQueue, &result, 0U) == pdTRUE;
}

void RfidService::taskEntry(void* context)
{
    static_cast<RfidService*>(context)->runTask();
}

void RfidService::runTask()
{
    while (_taskRunning) {
        pollDriver();
        vTaskDelay(pdMS_TO_TICKS(10U));
    }
    _taskHandle = nullptr;
    vTaskDelete(nullptr);
}

void RfidService::pollDriver()
{
    if (!_running) return;

    ++_updateCount;

    uint8_t uid[16]{};
    uint8_t uidLength = 0U;

    const bool uidAvailable = _driver.readUid(uid, uidLength, 500U);

    if (!uidAvailable) {
        if (_cardPresent) {
            _logger.info(logging::TAG_RFID, "Badge retire");
            clearUid();
        }
        return;
    }

    if (uidLength == 0U || uidLength > sizeof(uid)) {
        return;
    }

    ++_uidCount;

    if (_cardPresent && isSameUid(uid, uidLength)) {
        return;
    }

    char formattedUid[sizeof(_lastUid) * 3U]{};
    formatUid(uid, uidLength, formattedUid, sizeof(formattedUid));

    _logger.info(logging::TAG_RFID, "Nouveau badge detecte : %s", formattedUid);

    const uint64_t timestamp = utils::Timer::millis();
    const bool authorized = _authentication.authenticate(formattedUid, timestamp);

    Result result{};
    std::strncpy(result.uid, formattedUid, sizeof(result.uid) - 1U);
    result.authorized = authorized;
    result.authenticationResult = authorized
        ? types::AuthenticationResult::AUTHORIZED
        : types::AuthenticationResult::DENIED;
    result.timestamp = timestamp;

    if (_resultQueue != nullptr) {
        xQueueSend(_resultQueue, &result, 0U);
    }

    _publisher.publish({
        constants::EventID::RFID_CARD_READ,
        timestamp,
        0U
    });

    if (authorized) {
        _logger.info(logging::TAG_RFID, "Authentification : AUTORISEE");
    } else {
        _logger.warn(logging::TAG_RFID, "Authentification : REFUSEE (UID non autorise)");
    }

    rememberUid(uid, uidLength);
}

uint32_t RfidService::updateCount() const { return _updateCount; }
uint32_t RfidService::uidCount() const { return _uidCount; }
uint32_t RfidService::readFailureCount() const { return _readFailureCount; }

bool RfidService::isSameUid(const uint8_t* uid, uint8_t length) const
{
    if (uid == nullptr || length != _lastUidLength) return false;
    return std::memcmp(uid, _lastUid, length) == 0;
}

void RfidService::rememberUid(const uint8_t* uid, uint8_t length)
{
    if (uid == nullptr || length == 0U || length > sizeof(_lastUid)) return;
    std::memset(_lastUid, 0, sizeof(_lastUid));
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

void RfidService::formatUid(const uint8_t* uid, uint8_t length, char* output, uint8_t outputSize)
{
    if (uid == nullptr || output == nullptr || outputSize == 0U) return;
    output[0] = '\0';
    uint8_t offset = 0U;
    for (uint8_t index = 0U; index < length; ++index) {
        if (offset >= outputSize) break;
        const int written = std::snprintf(output + offset, outputSize - offset,
                                          "%s%02X", (index == 0U) ? "" : ":", uid[index]);
        if (written < 0 || static_cast<uint8_t>(written) >= (outputSize - offset)) {
            output[outputSize - 1U] = '\0';
            return;
        }
        offset += static_cast<uint8_t>(written);
    }
}

}  // namespace services
}  // namespace fortico