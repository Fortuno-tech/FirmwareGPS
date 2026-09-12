#pragma once

#include <cstddef>
#include <cstdint>

#include "interfaces/iservice.h"
#include "managers/telemetry_manager/telemetry_manager.h"
#include "services/mqtt/mqtt_context.h"
#include "services/gps/gps_service.h"
#include "drivers/rtc/rtc_driver.h"
#include "types/alert_types.h"
#include "types/auth_types.h"
#include "types/fuel_types.h"
#include "types/relay_types.h"
#include "services/temperature/temperature_sensor.h"

namespace fortico {
namespace services {

class ITelemetrySink {
public:
	virtual ~ITelemetrySink() = default;
	virtual bool publish(const char* payload, std::size_t length,
						bool highPriority = false) = 0;
	virtual bool publishChannel(const char* channel, const char* payload,
							   std::size_t length, bool highPriority = false)
	{
		(void)channel;
		(void)payload;
		(void)length;
		(void)highPriority;
		return false;
	}
};

class TelemetryService : public interfaces::IService {
public:
	TelemetryService(managers::TelemetryManager& manager,
					 ITelemetrySink* sink = nullptr,
					 const GpsService* gpsService = nullptr,
					 const drivers::RtcDriver* rtcDriver = nullptr,
					 ITemperatureSensor* temperatureSensor = nullptr);
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
	bool publishRfid(const char* uid, bool authorized,
					const drivers::RtcDateTime* eventTime = nullptr);
	bool publishEngine(types::RelayState relay, bool authorized);
	bool publishAlarm(const types::AlertEvent& alert);
	bool publishStatus(bool mqttConnected, bool simConnected,
					   bool gpsConnected, bool sdMounted);

private:
	managers::TelemetryManager& _manager;
	ITelemetrySink* _sink;
	const GpsService* _gpsService;
	const drivers::RtcDriver* _rtcDriver;
	ITemperatureSensor* _temperatureSensor;
	MqttDeviceContext _mqttContext;
	char _payload[1024];
	bool _running;
	uint32_t _publishIntervalMs;
	uint64_t _lastPublishMs;
	uint64_t _lastSlowPublishMs;
	bool _activeTracking;
};

}  // namespace services
}  // namespace fortico
