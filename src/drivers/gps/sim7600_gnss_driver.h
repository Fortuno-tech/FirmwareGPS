#pragma once

#include "core/logger/logger.h"
#include "drivers/sim7600/sim7600_driver.h"
#include "interfaces/igps_driver.h"

namespace fortico {
namespace drivers {

class Sim7600GnssDriver final : public interfaces::IGpsDriver {
public:
    Sim7600GnssDriver(Sim7600Driver& modem, core::Logger& logger);

    constants::ErrorCode init() override;
    constants::ErrorCode poll() override;
    bool start() override;
    bool stop() override;
    bool isHealthy() const override;
    bool isInitialized() const override;
    void reset() override;

    bool enableGnss() override;
    bool disableGnss() override;
    bool readPosition(types::GpsData& position) override;

private:
    bool isGnssEnabled();
    bool sendCommand(const char* command, char* response,
                     std::size_t responseSize, uint32_t timeoutMs);
    bool parseCgnssInfo(const char* response, types::GpsData& position,
                        const char*& failureReason) const;

    Sim7600Driver& _modem;
    core::Logger& _logger;
    bool _initialized;
    bool _healthy;
    bool _active;
    bool _gnssEnabled;
    bool _hasReportedFixState;
    bool _lastReportedFixValid;
};

}  // namespace drivers
}  // namespace fortico
