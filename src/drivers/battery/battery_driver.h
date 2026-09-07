#pragma once

#include "constants/errors.h"
#include "hal/adc/adc_hal.h"
#include "interfaces/idriver.h"

namespace fortico {
namespace drivers {

class BatteryDriver : public interfaces::IDriver {
public:
	explicit BatteryDriver(hal::AdcHal& adcHal);
	constants::ErrorCode init() override;
	constants::ErrorCode poll() override;
	bool start() override;
	bool stop() override;
	bool isHealthy() const override;
	bool isInitialized() const override;
	void reset() override;
	bool readVoltage(float& voltage) const;
	float readPercentage() const;
	bool isCritical() const;

private:
	hal::AdcHal& _adcHal;
	bool _initialized;
	bool _healthy;
	bool _active;
	float _voltage;
	float _percentage;
};

}  // namespace drivers
}  // namespace fortico
