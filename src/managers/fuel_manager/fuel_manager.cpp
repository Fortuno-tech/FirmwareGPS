#include "managers/fuel_manager/fuel_manager.h"

namespace fortico {
namespace managers {

FuelManager::FuelManager(drivers::FuelDriver& driver,
						 float tankCapacityLiters)
	: _driver(driver), _tankCapacityLiters(tankCapacityLiters), _level{},
	  _initialized(false)
{
	_level.percentage = -1.0F;
	_level.anomalyDetected = false;
}

void FuelManager::init()
{
	_initialized = (_driver.init() == constants::ErrorCode::OK ||
					_driver.isInitialized());
	update();
}

void FuelManager::update()
{
	if (!_initialized) return;
	_driver.poll();
	float voltage = 0.0F;
	if (!_driver.readVoltage(voltage)) {
		_level.percentage = -1.0F;
		_level.anomalyDetected = true;
		return;
	}
	const float normalized = voltage / 3.3F;
	_level.percentage = normalized < 0.0F ? 0.0F
						: (normalized > 1.0F ? 100.0F : normalized * 100.0F);
	_level.liters = _tankCapacityLiters * _level.percentage / 100.0F;
	_level.consumptionRateLph = 0.0F;
	_level.anomalyDetected = false;
}

void FuelManager::process() { update(); }

void FuelManager::reset()
{
	_driver.reset();
	_level = {};
	_level.percentage = -1.0F;
	_level.anomalyDetected = false;
}

types::FuelLevel FuelManager::getLevel() const { return _level; }
bool FuelManager::hasAnomaly() const { return _level.anomalyDetected; }

}  // namespace managers
}  // namespace fortico
