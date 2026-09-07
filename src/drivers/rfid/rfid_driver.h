#pragma once

#include <cstdint>

#include "constants/errors.h"
#include "hal/i2c/i2c_hal.h"
#include "interfaces/irfid_driver.h"

#ifdef ESP32_TARGET
#include <Adafruit_PN532.h>
#endif

namespace fortico {
namespace drivers {

class RfidDriver : public interfaces::IRfidDriver {
public:
	explicit RfidDriver(hal::I2cHal& i2cHal);

	constants::ErrorCode init() override;
	constants::ErrorCode poll() override;
	bool start() override;
	bool stop() override;
	bool isHealthy() const override;
	bool isInitialized() const override;
	void reset() override;

	bool readUid(uint8_t* uid, uint8_t& uidLength,
				 uint32_t timeoutMs) override;

private:
	hal::I2cHal& _i2cHal;
#ifdef ESP32_TARGET
	Adafruit_PN532 _pn532;
#endif
	bool _initialized;
	bool _healthy;
	bool _active;
};

}  // namespace drivers
}  // namespace fortico
