#pragma once

#include "core/diagnostics/diagnostics.h"
#include "core/error_handler/error_handler.h"
#include "core/event_bus/event_bus.h"
#include "core/power/power_monitor.h"
#include "drivers/sim7600/sim7600_driver.h"
#include "drivers/relay/relay_driver.h"
#include "drivers/fuel/fuel_driver.h"
#include "drivers/rtc/rtc_driver.h"
#include "drivers/sd/sd_driver.h"
#include "managers/alert_manager/alert_manager.hpp"
#include "managers/relay_manager/relay_manager.h"
#include "managers/fuel_manager/fuel_manager.h"
#include "managers/diagnostics_manager/diagnostics_manager.hpp"
#include "managers/ota_manager/ota_manager.h"
#include "managers/power_manager/power_manager.h"
#include "services/authentication/authentication_service.h"
#include "services/diagnostics/diagnostics_service.h"
#include "services/rfid/rfid_service.h"
#include "services/security/security_service.h"
#include "services/storage/storage_service.h"
#include "services/telemetry/telemetry_service.h"

namespace fortico {
namespace app {

class SystemController {
public:
	SystemController(core::EventBus& eventBus,
					 managers::AlertManager& alertManager,
					 services::AuthenticationService& authenticationService,
					 services::RfidService& rfidService,
					 drivers::Sim7600Driver& sim7600,
					 services::DiagnosticsService& diagnosticsService,
					 core::PowerMonitor& powerMonitor,
					 core::ErrorHandler& errorHandler,
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
					 drivers::SdDriver& sdDriver);

		bool init();
		bool start();
		void update();
		void stop();
		bool isInitialized() const;
		bool isRunning() const;
		core::PowerState powerState() const;
		bool hasFault() const;

private:
	core::EventBus& _eventBus;
	managers::AlertManager& _alertManager;
	services::AuthenticationService& _authenticationService;
	services::RfidService& _rfidService;
	drivers::Sim7600Driver& _sim7600;
	services::DiagnosticsService& _diagnosticsService;
	core::PowerMonitor& _powerMonitor;
	core::ErrorHandler& _errorHandler;
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
	bool _initialized;
	bool _running;
};

}  // namespace app
}  // namespace fortico
