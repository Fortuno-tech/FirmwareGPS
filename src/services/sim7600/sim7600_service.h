#pragma once

#include <cstddef>
#include <cstdint>

#include "drivers/sim7600/sim7600_driver.h"
#include "interfaces/iservice.h"

namespace fortico {
namespace services {

struct Sim7600Diagnostics {
    bool modemIdentified;
    bool simReady;
    bool signalAvailable;
    bool networkRegistered;
    bool operatorAvailable;
    bool radioServiceAvailable;
    bool packetAttached;
    bool dataConnected;
};

class Sim7600Service final : public interfaces::IService {
public:
    explicit Sim7600Service(drivers::Sim7600Driver& driver);

    bool start() override;
    void stop() override;
    bool isRunning() const override;

    bool waitForNetwork(uint32_t timeoutMs);
    bool connectInternet(const char* apn, uint32_t timeoutMs);
    bool runDiagnostics(Sim7600Diagnostics& diagnostics);
    bool getIpAddress(char* ipAddress, std::size_t ipAddressSize);

private:
    drivers::Sim7600Driver& _driver;
    bool _running;
};

}  // namespace services
}  // namespace fortico
