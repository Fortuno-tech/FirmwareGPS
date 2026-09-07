#include "drivers/rfid/rfid_driver.h"

#include "config/hardware_config.h"
#include "config/pin_config.h"

namespace fortico {
namespace drivers {

RfidDriver::RfidDriver(hal::I2cHal& i2cHal)
	: _i2cHal(i2cHal)
#ifdef ESP32_TARGET
	  , _pn532(pins::PN532_IRQ, pins::PN532_RESET, &i2cHal.native())
#endif
	  , _initialized(false), _healthy(false), _active(false)
{
}

constants::ErrorCode RfidDriver::init()
{
	if (_initialized) return constants::ErrorCode::OK;
#ifdef ESP32_TARGET
	if (!_i2cHal.init(pins::I2C_SDA, pins::I2C_SCL,
					  hardware::I2C_FREQUENCY_HZ)) {
		return constants::ErrorCode::I2C_BUS_TIMEOUT;
	}
	_pn532.begin();
	if (_pn532.getFirmwareVersion() == 0U) {
		return constants::ErrorCode::RFID_FAILURE;
	}
	_pn532.SAMConfig();
#else
	return constants::ErrorCode::RFID_FAILURE;
#endif
	_initialized = true;
	_healthy = true;
	_active = true;
	return constants::ErrorCode::OK;
}

constants::ErrorCode RfidDriver::poll()
{
	if (!_initialized) return constants::ErrorCode::NOT_INITIALIZED;
	return _healthy ? constants::ErrorCode::OK : constants::ErrorCode::RFID_FAILURE;
}

bool RfidDriver::start()
{
	if (!_initialized) return false;
	_active = true;
	return true;
}

bool RfidDriver::stop()
{
	if (!_initialized) return false;
	_active = false;
	return true;
}

bool RfidDriver::isHealthy() const { return _initialized && _healthy; }
bool RfidDriver::isInitialized() const { return _initialized; }

void RfidDriver::reset()
{
	_active = _initialized;
	_healthy = _initialized;
}

bool RfidDriver::readUid(uint8_t* uid, uint8_t& uidLength,
						 uint32_t timeoutMs)
{
	uidLength = 0U;
	if (!_initialized || !_active || !_healthy || uid == nullptr) return false;
#ifdef ESP32_TARGET
	return _pn532.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid,
									  &uidLength,
									  static_cast<uint16_t>(timeoutMs));
#else
	(void)timeoutMs;
	return false;
#endif
}

}  // namespace drivers
}  // namespace fortico
