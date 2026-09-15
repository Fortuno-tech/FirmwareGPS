#pragma once

#include <cstdint>

#include "constants/errors.h"
#include "hal/i2c/i2c_hal.h"
#include "interfaces/idriver.h"

namespace fortico {
namespace drivers {

struct RtcDateTime {
	uint16_t year;
	uint8_t month;
	uint8_t day;
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
};

class RtcDriver : public interfaces::IDriver {
public:
	explicit RtcDriver(hal::I2cHal& i2cHal);
	constants::ErrorCode init() override;
	constants::ErrorCode poll() override;
	bool start() override;
	bool stop() override;
	bool isHealthy() const override;
	bool isInitialized() const override;
	void reset() override;
	bool readDateTime(RtcDateTime& dateTime);
	bool setDateTime(const RtcDateTime& dateTime);
	uint64_t timestamp() const;

private:
	static uint8_t bcdToDecimal(uint8_t value);
	static uint8_t decimalToBcd(uint8_t value);
	bool valid(const RtcDateTime& dateTime) const;
	hal::I2cHal& _i2cHal;
	bool _initialized;
	bool _healthy;
	bool _active;
	uint64_t _lastPollMs;
};

}  // namespace drivers
}  // namespace fortico
