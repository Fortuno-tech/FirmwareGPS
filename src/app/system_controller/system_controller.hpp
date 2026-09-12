#pragma once

#include <cstdint>

#include "core/diagnostics/diagnostics.h"
#include "core/error_handler/error_handler.h"
#include "core/event_bus/event_bus.h"
#include "core/logger/logger.h"
#include "core/power/power_monitor.h"
#include "drivers/relay/relay_driver.h"
#include "drivers/fuel/fuel_driver.h"
#include "drivers/rtc/rtc_driver.h"
#include "drivers/sd/sd_driver.h"
#include "drivers/sim7600/sim7600_power_driver.h"
#include "managers/alert_manager/alert_manager.hpp"
#include "managers/relay_manager/relay_manager.h"
#include "managers/fuel_manager/fuel_manager.h"
#include "managers/diagnostics_manager/diagnostics_manager.hpp"
#include "managers/ota_manager/ota_manager.h"
#include "managers/power_manager/power_manager.h"
#include "middleware/mqtt_client/cellular_mqtt_client.h"
#include "services/authentication/authentication_service.h"
#include "services/diagnostics/diagnostics_service.h"
#include "services/gps/gps_service.h"
#include "services/rfid/rfid_service.h"
#include "services/security/security_service.h"
#include "services/sim7600/sim7600_service.h"
#include "services/storage/storage_service.h"
#include "services/telemetry/telemetry_service.h"
#include "types/alert_types.h"

namespace fortico {
namespace app {

enum class BootStage : uint8_t {
	Idle,
	Starting,
	PoweringModem,
	InitializingModem,
	InitializingGnss,
	RegisteringNetwork,
	ConnectingMqtt,
	StartingServices,
	Ready,
	Failed,
};

class SystemController {
public:
	SystemController(core::EventBus& eventBus,
					 managers::AlertManager& alertManager,
					 services::AuthenticationService& authenticationService,
					 services::RfidService& rfidService,
					 drivers::Sim7600PowerDriver& sim7600PowerDriver,
					 services::Sim7600Service& sim7600Service,
					 services::GpsService& gpsService,
					 services::DiagnosticsService& diagnosticsService,
					 core::PowerMonitor& powerMonitor,
					 core::ErrorHandler& errorHandler,
					 core::Logger& logger,
					 services::SecurityService& securityService,
					 services::StorageService& storageService,
					 services::TelemetryService& telemetryService,
					 managers::PowerManager& powerManager,
					 managers::OtaManager& otaManager,
					 drivers::RelayDriver& relayDriver,
					 managers::RelayManager& relayManager,
					 drivers::FuelDriver& fuelDriver,
					 managers::FuelManager& fuelManager,
					 drivers::RtcDriver& rtcDriver,
					 drivers::SdDriver& sdDriver,
					 middleware::CellularMqttClient& mqttClient);

		bool init();
		bool start();
		void update();
		void stop();
		bool isInitialized() const;
		bool isRunning() const;
		core::PowerState powerState() const;
		bool hasFault() const;
		BootStage bootStage() const;

private:
	core::EventBus& _eventBus;
	managers::AlertManager& _alertManager;
	services::AuthenticationService& _authenticationService;
	services::RfidService& _rfidService;
	drivers::Sim7600PowerDriver& _sim7600PowerDriver;
	services::Sim7600Service& _sim7600Service;
	services::GpsService& _gpsService;
	services::DiagnosticsService& _diagnosticsService;
	core::PowerMonitor& _powerMonitor;
	core::ErrorHandler& _errorHandler;
	core::Logger& _logger;
	services::SecurityService& _securityService;
	services::StorageService& _storageService;
	services::TelemetryService& _telemetryService;
	managers::PowerManager& _powerManager;
	managers::OtaManager& _otaManager;
	drivers::RelayDriver& _relayDriver;
	managers::RelayManager& _relayManager;
	drivers::FuelDriver& _fuelDriver;
	managers::FuelManager& _fuelManager;
	drivers::RtcDriver& _rtcDriver;
	drivers::SdDriver& _sdDriver;
	middleware::CellularMqttClient& _mqttClient;
	bool _initialized;
	bool _running;
	BootStage _bootStage;
	uint64_t _lastNetworkTimeSyncMs;
	bool _lastRfidHealthy;
	bool _lastSdHealthy;
	bool _healthStateKnown;
	uint64_t _lastHeartbeatMs;
	uint64_t _lastAuthenticationTimestamp;
	uint64_t _lastGpsLogMs;
	uint64_t _lastAlertTimestamp;
	types::AlertCode _lastAlertCode;
	bool _hasProcessedAlert;
	uint64_t _lastBatteryLogMs;
	uint64_t _lastStatusPublishMs;
	types::RelayState _lastPublishedRelayState;
	bool _relayStateKnown;

	void setBootStage(BootStage stage, const char* message);
	bool failBoot(const char* message, constants::ErrorCode error);
	void logSim7600Diagnostics();
	void logGpsPosition();
	void logBatteryStatus();
	void processAlerts();
	void synchronizeRtcFromNetwork();
};

}  // namespace app
}  // namespace fortico
