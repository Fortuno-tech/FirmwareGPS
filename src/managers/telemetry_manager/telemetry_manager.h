#pragma once

#include "drivers/battery/battery_driver.h"
#include "interfaces/imanager.h"
#include "interfaces/ipublisher.h"
#include "types/telemetry_types.h"

namespace fortico {
namespace managers {

class TelemetryManager : public interfaces::IManager {
public:
	TelemetryManager(drivers::BatteryDriver& battery,
					 interfaces::IPublisher* publisher = nullptr);
	void init() override;
	void update() override;
	void process() override;
	void reset() override;
	void setFuel(const types::FuelLevel& fuel);
	void setGps(const types::GpsData& gps);
	void setRelay(types::RelayState relay);
	void setAuthentication(types::AuthenticationResult result);
	void setTemperature(float temperatureC);
	types::TelemetryPacket packet() const;

private:
	drivers::BatteryDriver& _battery;
	interfaces::IPublisher* _publisher;
	types::TelemetryPacket _packet;
	bool _initialized;
};

}  // namespace managers
}  // namespace fortico
