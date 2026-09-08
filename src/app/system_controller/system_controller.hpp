#pragma once

#include "core/diagnostics/diagnostics.h"
#include "core/error_handler/error_handler.h"
#include "core/event_bus/event_bus.h"
#include "core/power/power_monitor.h"
#include "drivers/sim7600/sim7600_driver.h"
#include "managers/alert_manager/alert_manager.hpp"
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
					 managers::OtaManager& otaManager);

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
	bool _initialized;
	bool _running;
};

}  // namespace app
}  // namespace fortico
