#pragma once

#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "core/logger/logger.h"
#include "interfaces/ipublisher.h"
#include "interfaces/irfid_driver.h"
#include "interfaces/iservice.h"
#include "services/authentication/authentication_service.h"

namespace fortico {
namespace services {

class RfidService : public interfaces::IService {
public:
    struct Result {
        char uid[16];
        bool authorized;
        types::AuthenticationResult authenticationResult;
        uint64_t timestamp;
    };

    RfidService(
        interfaces::IRfidDriver& driver,
        AuthenticationService& authentication,
        interfaces::IPublisher& publisher,
        core::Logger& logger); // Injecter le logger ici

    bool start() override;
    void stop() override;
    bool isRunning() const override;

    void update();

    uint32_t updateCount() const;
    uint32_t uidCount() const;
    uint32_t readFailureCount() const;
    bool receiveResult(Result& result);

private:
    interfaces::IRfidDriver& _driver;
    AuthenticationService& _authentication;
    interfaces::IPublisher& _publisher;
    core::Logger& _logger; // Référence vers le logger système

    bool _running;
    uint8_t _lastUid[16];
    uint8_t _lastUidLength;
    bool _cardPresent;
    uint32_t _nextPollMs;
    uint32_t _updateCount;
    uint32_t _uidCount;
    uint32_t _readFailureCount;
    QueueHandle_t _resultQueue;
    TaskHandle_t _taskHandle;
    volatile bool _taskRunning;

    static void taskEntry(void* context);
    void runTask();
    void pollDriver();

    bool isSameUid(const uint8_t* uid, uint8_t length) const;
    void rememberUid(const uint8_t* uid, uint8_t length);
    void clearUid();

    static void formatUid(
        const uint8_t* uid,
        uint8_t length,
        char* output,
        uint8_t outputSize);
};

}  // namespace services
}  // namespace fortico