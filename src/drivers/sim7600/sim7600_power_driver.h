#pragma once

#include "hal/gpio/gpio_hal.h"
#include "hal/delay/delay_hal.h"
#include "interfaces/idriver.h"

namespace fortico {
namespace drivers {

class Sim7600PowerDriver final : public interfaces::IDriver {
public:
	Sim7600PowerDriver(hal::GpioHal& gpioHal, hal::DelayHal& delayHal);

    constants::ErrorCode init() override;
    constants::ErrorCode poll() override;
    bool start() override;
    bool stop() override;
    bool isHealthy() const override;
    bool isInitialized() const override;
    void reset() override;

    /// Demande l'allumage par une unique impulsion K/PWRKEY active bas.
    bool powerOn();
    void setPowerOnIssued(bool issued = true);
    bool isPowerOnIssued() const;

private:
	hal::GpioHal& _gpioHal;
	hal::DelayHal& _delayHal;
    bool _initialized;
    bool _healthy;
    bool _active;
    bool _powerOnIssued;
};

}  // namespace drivers
}  // namespace fortico
