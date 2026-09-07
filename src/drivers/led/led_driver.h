#pragma once

#include "constants/errors.h"
#include "hal/gpio/gpio_hal.h"
#include "interfaces/idriver.h"

namespace fortico {
namespace drivers {

class LedDriver : public interfaces::IDriver {
public:
	explicit LedDriver(hal::GpioHal& gpioHal);

	constants::ErrorCode init() override;
	constants::ErrorCode poll() override;
	bool start() override;
	bool stop() override;
	bool isHealthy() const override;
	bool isInitialized() const override;
	void reset() override;

	bool set(bool enabled);
	bool on();
	bool off();
	bool toggle();
	bool isOn() const;

private:
	hal::GpioHal& _gpioHal;
	bool _initialized;
	bool _healthy;
	bool _active;
	bool _enabled;
};

}  // namespace drivers
}  // namespace fortico
