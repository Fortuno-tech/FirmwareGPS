#pragma once

#include "interfaces/imanager.h"
#include "drivers/fuel/fuel_driver.h"
#include "types/fuel_types.h"

namespace fortico {
namespace managers {

class FuelManager : public interfaces::IManager {
public:
	explicit FuelManager(drivers::FuelDriver& driver,
						  float tankCapacityLiters = 100.0F);

	void init() override;
	void update() override;
	void process() override;
	void reset() override;

	types::FuelLevel getLevel() const;
	bool hasAnomaly() const;

private:
	drivers::FuelDriver& _driver;
	float _tankCapacityLiters;
	types::FuelLevel _level;
	bool _initialized;
};

}  // namespace managers
}  // namespace fortico
