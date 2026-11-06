#pragma once

#include <cstdint>

#if defined(FORTICO_ENABLE_CONTROL_TIMING) && FORTICO_ENABLE_CONTROL_TIMING
#include "app/control_timing.hpp"
#endif

#include "app/connectivity_task/connectivity_task.hpp"
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
#include "app/system_controller/remote_relay_command_mailbox.hpp"
#include "app/system_controller/remote_command_execution.hpp"
#include "app/system_controller/system_event_mailbox.hpp"
#include "services/authentication/authentication_service.h"
#include "services/diagnostics/diagnostics_service.h"
#include "services/gps/gps_service.h"
#include "services/rfid/rfid_service.h"
#include "services/security/security_service.h"
#include "services/sim7600/sim7600_service.h"
#include "services/storage/storage_service.h"
#include "services/telemetry/telemetry_service.h"
#include "interfaces/ievent_listener.h"
#include "types/alert_types.h"
#include "types/system_types.h"

namespace fortico {
namespace app {

enum class BootStage : uint8_t {
	Idle,
	CheckProvisioning,
	InitHardware,
	StartingServices,
	PoweringModem,
	InitializingModem,
	InitializingGnss,
	RegisteringNetwork,
	ConnectingMqtt,
	Ready,
	Failed,
};

class SystemController : public interfaces::IEventListener {
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
					 middleware::CellularMqttClient& mqttClient,
					 ConnectivityTask& connectivityTask);

		bool init();
		bool start();
		void update();
		void stop();
		void onEvent(const types::Event& event) override;
		bool isInitialized() const;
		bool isRunning() const;
		core::PowerState powerState() const;
		bool hasFault() const;
		BootStage bootStage() const;
		types::SystemState systemState() const;
		void transitionTo(types::SystemState newState);
		bool submitRemoteRelayCommand(types::RelayCmd command,
									  uint64_t timestamp);
#if defined(FORTICO_ENABLE_CONTROL_TIMING) && FORTICO_ENABLE_CONTROL_TIMING
		void recordControlCycle(uint32_t startedAtMs, uint32_t completedAtMs);
#endif

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
	ConnectivityTask& _connectivityTask;
	RemoteRelayCommandMailbox& _remoteRelayCommandMailbox;
	SystemEventMailbox& _systemEventMailbox;
	RemoteCommandExecution _remoteCommandExecution;
	bool _initialized;
	bool _running;
	BootStage _bootStage;
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

	types::SystemState _systemState;
	uint64_t _lastGpsMqttPublishMs;
	bool _killLocked;
	bool _lastMqttConnected;
#if defined(FORTICO_ENABLE_CONTROL_TIMING) && FORTICO_ENABLE_CONTROL_TIMING
	ControlTimingMetrics _controlTiming;
	bool _hasPendingRemoteActionTiming;
	uint32_t _pendingRemoteReceiptMs;
	types::RelayCommand _pendingRemoteAction;
	uint32_t _lastControlTimingLogMs;
#endif

	void setBootStage(BootStage stage, const char* message);
	bool failBoot(const char* message, constants::ErrorCode error);
	void logGpsPosition();
	void logBatteryStatus();
	void processAlerts();
	void processSystemEvents();
	void processEvent(const types::Event& event);
	void processRemoteRelayCommands();
	void beginRemoteExecution(const RemoteRelayCommand& command,
							  bool accepted,
							  types::RelayState expectedRelayState,
							  const char* error);
	void publishRemoteExecutionResult();
#if defined(FORTICO_ENABLE_CONTROL_TIMING) && FORTICO_ENABLE_CONTROL_TIMING
	void trackRemoteRelaySubmission(const RemoteRelayCommand& remoteCommand,
									const types::RelayCommand& action,
									bool submitted);
#endif
};

}  // namespace app
}  // namespace fortico
