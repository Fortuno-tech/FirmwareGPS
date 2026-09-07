#pragma once

#include <cstdint>

#include "interfaces/ipublisher.h"
#include "interfaces/irfid_driver.h"
#include "interfaces/iservice.h"
#include "services/authentication/authentication_service.h"

namespace fortico {
namespace services {

class RfidService : public interfaces::IService {
public:
    RfidService(interfaces::IRfidDriver& driver,
                AuthenticationService& authentication,
                interfaces::IPublisher& publisher);

    bool start() override;
    void stop() override;
    bool isRunning() const override;
    void update();

private:
    interfaces::IRfidDriver& _driver;
    AuthenticationService& _authentication;
    interfaces::IPublisher& _publisher;
    bool _running;
    uint8_t _lastUid[16];
    uint8_t _lastUidLength;
    bool _cardPresent;

    bool isSameUid(const uint8_t* uid, uint8_t length) const;
    void rememberUid(const uint8_t* uid, uint8_t length);
    void clearUid();
    static void formatUid(const uint8_t* uid, uint8_t length,
                          char* output, uint8_t outputSize);
};

}  // namespace services
}  // namespace fortico