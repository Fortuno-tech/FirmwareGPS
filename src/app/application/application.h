#pragma once

#include "app/app_state_machine/app_state_machine.hpp"
#include "app/system_controller/system_controller.hpp"
#include "app/task_manager/task_manager.hpp"
#include "core/event_bus/event_bus.h"
#include "interfaces/irfid_driver.h"
#include "drivers/sim7600/sim7600_driver.h"
#include "managers/alert_manager/alert_manager.hpp"
#include "services/authentication/authentication_service.h"
#include "services/rfid/rfid_service.h"
#include "services/diagnostics/diagnostics_service.h"
#include "core/power/power_monitor.h"
#include "core/error_handler/error_handler.h"
#include "services/security/security_service.h"
#include "services/storage/storage_service.h"
#include "services/telemetry/telemetry_service.h"
#include "managers/power_manager/power_manager.h"
#include "managers/ota_manager/ota_manager.h"

namespace fortico {
namespace app {

class Application {
public:
	Application(core::EventBus& eventBus,
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
				drivers::SdDriver& sdDriver,
				middleware::CellularMqttClient& mqttClient);

	bool init();
	bool start();
	void update();
	void stop();
	types::SystemState state() const;

private:
	static void updateTask(void* context);
	void scheduledUpdate();

	SystemController _systemController;
	TaskManager _taskManager;
	AppStateMachine _stateMachine;
	bool _initialized;
	bool _running;
};

}  // namespace app
}  // namespace fortico
