#include "drivers/rtc/rtc_driver.h"

#include <Arduino.h>

#include "config/hardware_config.h"
#include "config/pin_config.h"
#include "utils/timer_utils.h"

namespace fortico {
namespace drivers {
namespace { constexpr uint8_t kAddress = 0x68U; }

RtcDriver::RtcDriver(hal::I2cHal& i2cHal)
	: _i2cHal(i2cHal), _initialized(false), _healthy(false), _active(false), _lastPollMs(0U) {}

constants::ErrorCode RtcDriver::init()
{
	if (_initialized) return constants::ErrorCode::OK;

    // Ne pas réinitialiser _i2cHal.init() ici : le bus est déjà initialisé dans setup()
    if (!_i2cHal.lock(hardware::I2C_MUTEX_TIMEOUT_MS)) {
        return constants::ErrorCode::I2C_BUS_TIMEOUT;
	}

    uint8_t probe = 0U;
    const bool ok = _i2cHal.write(kAddress, &probe, 0U, hardware::I2C_TRANSACTION_MAX_MS);
	_i2cHal.unlock();

    if (!ok) return constants::ErrorCode::RTC_FAILURE;
    _initialized = true;
    _active = true;
    _healthy = true;
    return constants::ErrorCode::OK;
}

bool RtcDriver::readDateTime(RtcDateTime& dateTime)
{
	if (!_initialized || !_active) return false;

    // Verrouillage du mutex I2C indispensable
	if (!_i2cHal.lock(hardware::I2C_MUTEX_TIMEOUT_MS)) {
        return false;
    }

    uint8_t reg = 0U;
    uint8_t data[7]{};
    const bool ok = _i2cHal.writeRead(kAddress, &reg, 1U, data, sizeof(data), hardware::I2C_TRANSACTION_MAX_MS);
	_i2cHal.unlock();

	if (!ok) return false;

    dateTime.second = bcdToDecimal(data[0] & 0x7FU);
    dateTime.minute = bcdToDecimal(data[1] & 0x7FU);
    dateTime.hour = bcdToDecimal(data[2] & 0x3FU);
    dateTime.day = bcdToDecimal(data[4] & 0x3FU);
    dateTime.month = bcdToDecimal(data[5] & 0x1FU);
    dateTime.year = static_cast<uint16_t>(2000U + bcdToDecimal(data[6]));
    return valid(dateTime);
}
constants::ErrorCode RtcDriver::poll()
{
	if (!_initialized) return constants::ErrorCode::NOT_INITIALIZED;
	if (!_active) return constants::ErrorCode::OK;
	const uint64_t now = utils::Timer::millis();
	if (now - _lastPollMs < hardware::RTC_POLL_INTERVAL_MS) {
		return _healthy ? constants::ErrorCode::OK : constants::ErrorCode::RTC_FAILURE;
	}
	_lastPollMs = now;
	RtcDateTime value{};
	_healthy = readDateTime(value);
	return _healthy ? constants::ErrorCode::OK : constants::ErrorCode::RTC_FAILURE;
}

bool RtcDriver::start() { return _initialized ? (_active = true) : false; }
bool RtcDriver::stop() { return _initialized ? (_active = false, true) : false; }
bool RtcDriver::isHealthy() const { return _initialized && _healthy; }
bool RtcDriver::isInitialized() const { return _initialized; }
void RtcDriver::reset() { _healthy = _initialized; _active = _initialized; _lastPollMs = 0U; }

bool RtcDriver::setDateTime(const RtcDateTime& value)
{
	if (!_initialized || !_active || !valid(value)) return false;
	if (!_i2cHal.lock(hardware::I2C_MUTEX_TIMEOUT_MS)) {
        return false;
    }
	uint8_t data[8] = {0U, decimalToBcd(value.second), decimalToBcd(value.minute),
		decimalToBcd(value.hour), 1U, decimalToBcd(value.day),
		decimalToBcd(value.month), decimalToBcd(static_cast<uint8_t>(value.year - 2000U))};
	const bool success = _i2cHal.write(kAddress, data, sizeof(data), hardware::I2C_TRANSACTION_MAX_MS);
	_i2cHal.unlock();
	return success;
}

uint64_t RtcDriver::timestamp() const
{
	if (!_initialized || !_active) return 0U;
	RtcDateTime value{};
	if (!const_cast<RtcDriver*>(this)->readDateTime(value)) return 0U;
	uint64_t days = 0U;
	for (uint16_t year = 1970U; year < value.year; ++year) {
		days += ((year % 4U == 0U && year % 100U != 0U) ||
				 (year % 400U == 0U)) ? 366U : 365U;
	}
	static constexpr uint8_t daysPerMonth[] =
		{31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U};
	for (uint8_t month = 1U; month < value.month; ++month) {
		days += daysPerMonth[month - 1U];
		if (month == 2U && value.year % 4U == 0U &&
			(value.year % 100U != 0U || value.year % 400U == 0U)) {
			++days;
		}
	}
	days += static_cast<uint64_t>(value.day - 1U);
	return days * 86400ULL + static_cast<uint64_t>(value.hour) * 3600ULL +
		   static_cast<uint64_t>(value.minute) * 60ULL + value.second;
}
uint8_t RtcDriver::bcdToDecimal(uint8_t value) { return static_cast<uint8_t>((value >> 4U) * 10U + (value & 0x0FU)); }
uint8_t RtcDriver::decimalToBcd(uint8_t value) { return static_cast<uint8_t>((value / 10U << 4U) | (value % 10U)); }
bool RtcDriver::valid(const RtcDateTime& value) const
{
	return value.year >= 2000U && value.year <= 2099U && value.month >= 1U &&
		   value.month <= 12U && value.day >= 1U && value.day <= 31U &&
		   value.hour <= 23U && value.minute <= 59U && value.second <= 59U;
}

}  // namespace drivers
}  // namespace fortico
