#pragma once

#include <cstddef>
#include <cstdint>

#include "interfaces/iservice.h"
#include "managers/telemetry_manager/telemetry_manager.h"
#include "services/gps/gps_service.h"
#include "types/auth_types.h"
#include "types/fuel_types.h"
#include "types/relay_types.h"

namespace fortico {
namespace services {

class ITelemetrySink {
public:
	virtual ~ITelemetrySink() = default;
	virtual bool publish(const char* payload, std::size_t length) = 0;
};

class TelemetryService : public interfaces::IService {
public:
	TelemetryService(managers::TelemetryManager& manager,
					 ITelemetrySink* sink = nullptr,
					 const GpsService* gpsService = nullptr);
	bool start() override;
	void stop() override;
	bool isRunning() const override;
	void update();
	const char* lastPayload() const;

	void setActiveTracking(bool active);
	bool isActiveTracking() const;
	void setPublishInterval(uint32_t intervalMs);
	bool forcePublish();

	void setRelay(types::RelayState relay);
	void setAuthentication(types::AuthenticationResult result);
	void setFuel(const types::FuelLevel& fuel);
	void setIdentity(const char* deviceId, const char* vehicleId);
	void setTimestampIso(const char* timestampIso);

private:
	managers::TelemetryManager& _manager;
	ITelemetrySink* _sink;
	const GpsService* _gpsService;
	char _payload[1024];
	bool _running;
	uint32_t _publishIntervalMs;
	char _deviceId[32];
	char _vehicleId[32];
	char _timestampIso[32];
	uint64_t _lastPublishMs;
	bool _activeTracking;
};

}  // namespace services
}  // namespace fortico
