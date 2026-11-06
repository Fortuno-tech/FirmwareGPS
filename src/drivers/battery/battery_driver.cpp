#include "drivers/battery/battery_driver.h"

#include "config/hardware_config.h"
#include "config/pin_config.h"

namespace fortico {
namespace drivers {

BatteryDriver::BatteryDriver(hal::AdcHal& adcHal)
	: _adcHal(adcHal), _initialized(false), _healthy(false), _active(false),
	  _voltage(0.0F), _percentage(0.0F)
{
}

constants::ErrorCode BatteryDriver::init()
{
	if (_initialized) return constants::ErrorCode::OK;
	if (!_adcHal.init(pins::BATTERY_ADC, hardware::ADC_ATTENUATION_DB)) {
		return constants::ErrorCode::BATTERY_SENSOR_FAILURE;
	}
	_initialized = true;
	_active = true;
	return poll();
}

constants::ErrorCode BatteryDriver::poll()
{
	if (!_initialized) return constants::ErrorCode::NOT_INITIALIZED;
	if (!_active) return constants::ErrorCode::OK;
	_voltage = (_adcHal.readAvg(hardware::ADC_OVERSAMPLE_COUNT) / 1000.0F) *
			   hardware::BATTERY_VOLTAGE_DIVIDER_RATIO;
	_percentage = (_voltage - 10.5F) * (100.0F / (12.6F - 10.5F));
	if (_percentage < 0.0F) _percentage = 0.0F;
	if (_percentage > 100.0F) _percentage = 100.0F;
	_healthy = _voltage > 0.1F && _voltage < 20.0F;
	return _healthy ? constants::ErrorCode::OK
					: constants::ErrorCode::BATTERY_SENSOR_FAILURE;
}

bool BatteryDriver::start() { return _initialized ? (_active = true) : false; }
bool BatteryDriver::stop() { return _initialized ? (_active = false, true) : false; }
bool BatteryDriver::isHealthy() const { return _initialized && _healthy; }
bool BatteryDriver::isInitialized() const { return _initialized; }
void BatteryDriver::reset() { _healthy = _initialized; _active = _initialized; }
bool BatteryDriver::readVoltage(float& voltage) const
{
	if (!isHealthy() || !_active) return false;
	voltage = _voltage;
	return true;
}
float BatteryDriver::readPercentage() const { return _percentage; }
bool BatteryDriver::isCritical() const
{
	return _initialized && _voltage < hardware::BATTERY_CRITICAL_VOLTAGE;
}

}  // namespace drivers
}  // namespace fortico
