#pragma once

#include "constants/errors.h"
#include "hal/gpio/gpio_hal.h"
#include "interfaces/idriver.h"

namespace fortico {
namespace drivers {

class RelayDriver : public interfaces::IDriver {
public:
	explicit RelayDriver(hal::GpioHal& gpioHal);

	constants::ErrorCode init() override;
	constants::ErrorCode poll() override;
	bool start() override;
	bool stop() override;
	bool isHealthy() const override;
	bool isInitialized() const override;
	void reset() override;

	bool set(bool engaged);
	bool engage();
	bool cut();
	bool isEngaged() const;

private:
	hal::GpioHal& _gpioHal;
	bool _initialized;
	bool _healthy;
	bool _active;
	bool _engaged;
};

}  // namespace drivers
}  // namespace fortico
