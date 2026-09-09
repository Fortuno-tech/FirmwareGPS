#pragma once

#include "interfaces/igps_driver.h"
#include "interfaces/iservice.h"
#include "types/gps_types.h"

namespace fortico {
namespace services {

class GpsService final : public interfaces::IService {
public:
    explicit GpsService(interfaces::IGpsDriver& driver);

    bool start() override;
    void stop() override;
    bool isRunning() const override;
    void update();
    const types::GpsData& data() const;

private:
    interfaces::IGpsDriver& _driver;
    types::GpsData _data;
    uint32_t _lastPollMs;
    bool _running;
};

}  // namespace services
}  // namespace fortico
