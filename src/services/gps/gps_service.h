#pragma once

#include <atomic>

#include <freertos/FreeRTOS.h>

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
    types::GpsData data() const;

private:
    void setData(const types::GpsData& data);

    interfaces::IGpsDriver& _driver;
    types::GpsData _data;
    uint32_t _lastPollMs;
    std::atomic<bool> _running;
    mutable portMUX_TYPE _dataMux = portMUX_INITIALIZER_UNLOCKED;
};

}  // namespace services
}  // namespace fortico
