#pragma once

#include <cstdint>

#include "constants/errors.h"
#include "hal/gpio/gpio_hal.h"
#include "interfaces/idriver.h"

namespace fortico {
namespace drivers {

class ButtonDriver : public interfaces::IDriver {
public:
	explicit ButtonDriver(hal::GpioHal& gpioHal);

	constants::ErrorCode init() override;
	constants::ErrorCode poll() override;
	bool start() override;
	bool stop() override;
	bool isHealthy() const override;
	bool isInitialized() const override;
	void reset() override;

	bool isPressed() const;
	bool wasPressed();
	bool wasLongPressed();

private:
	hal::GpioHal& _gpioHal;
	bool _initialized;
	bool _healthy;
	bool _active;
	bool _stablePressed;
	bool _candidatePressed;
	bool _pressEvent;
	bool _longPressEvent;
	bool _longPressReported;
	uint32_t _candidateSinceMs;
	uint32_t _pressedSinceMs;
};

}  // namespace drivers
}  // namespace fortico
