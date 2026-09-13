#pragma once

#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

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
        interfaces::IPublisher& publisher);

    bool start() override;
    void stop() override;
    bool isRunning() const override;

    // Dépile au plus un résultat produit par RfidTask, sans bloquer.
    void update();

    uint32_t updateCount() const;
    uint32_t uidCount() const;
    uint32_t readFailureCount() const;
	bool receiveResult(Result& result);

private:

    interfaces::IRfidDriver& _driver;
    AuthenticationService& _authentication;
    interfaces::IPublisher& _publisher;

    bool _running;

    /*
     * Dernier UID connu.
     */
    uint8_t _lastUid[16];
    uint8_t _lastUidLength;

    /*
     * Indique si un badge est actuellement considéré
     * comme présent.
     */
    bool _cardPresent;

    uint32_t _nextPollMs;

    uint32_t _updateCount;
    uint32_t _uidCount;

    /*
     * Vraies erreurs de lecture.
     *
     * Une absence normale de badge ne sera plus
     * comptée comme une erreur.
     */
    uint32_t _readFailureCount;
    QueueHandle_t _resultQueue;
    TaskHandle_t _taskHandle;
    volatile bool _taskRunning;

    static void taskEntry(void* context);
    void runTask();
    void pollDriver();

    bool isSameUid(
        const uint8_t* uid,
        uint8_t length) const;

    void rememberUid(
        const uint8_t* uid,
        uint8_t length);

    void clearUid();

    static void formatUid(
        const uint8_t* uid,
        uint8_t length,
        char* output,
        uint8_t outputSize);
};

}  // namespace services
}  // namespace fortico