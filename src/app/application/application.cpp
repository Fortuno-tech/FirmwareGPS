#include "app/application/application.h"

namespace fortico {
namespace app {

Application::Application(core::EventBus& eventBus,
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
						 managers::OtaManager& otaManager)
	: _eventBus(eventBus), _alertManager(alertManager),
				  _authenticationService(authenticationService), _rfidService(rfidService),
				  _sim7600(sim7600),
				  _diagnosticsService(diagnosticsService), _powerMonitor(powerMonitor),
				  _errorHandler(errorHandler),
				  _securityService(securityService), _storageService(storageService),
				  _telemetryService(telemetryService),
				  _powerManager(powerManager), _otaManager(otaManager),
	  _initialized(false), _running(false)
{
}

bool Application::init()
{
	if (_initialized) return true;
	if (!_eventBus.init()) {
		_errorHandler.report(constants::ErrorCode::UNKNOWN);
		return false;
	}
	if (!_powerMonitor.init()) {
		_errorHandler.report(constants::ErrorCode::BATTERY_SENSOR_FAILURE);
		return false;
	}
	_powerManager.init();
	_otaManager.init();
	if (!_securityService.start()) {
		_errorHandler.report(constants::ErrorCode::NVS_READ_FAILED);
		return false;
	}
	if (!_storageService.start()) {
		_errorHandler.report(constants::ErrorCode::NVS_READ_FAILED);
		_securityService.stop();
		return false;
	}
	_alertManager.init();
	_alertManager.subscribeTo(_eventBus);
	_initialized = true;
	return true;
}

bool Application::start()
{
	if (!_initialized) return false;
	if (!_authenticationService.start()) {
		_errorHandler.report(constants::ErrorCode::UNKNOWN);
		return false;
	}
	if (!_telemetryService.start()) {
		_storageService.stop();
		_securityService.stop();
		_authenticationService.stop();
		return false;
	}
	if (!_diagnosticsService.start()) {
		_authenticationService.stop();
		return false;
	}
	if (_sim7600.init() != constants::ErrorCode::OK || !_sim7600.start()) {
		_errorHandler.report(constants::ErrorCode::MODEM_AT_TIMEOUT);
		_authenticationService.stop();
		_diagnosticsService.stop();
		_telemetryService.stop();
		_storageService.stop();
		_securityService.stop();
		return false;
	}
	if (!_rfidService.start()) {
		_sim7600.stop();
		_authenticationService.stop();
		_diagnosticsService.stop();
		_telemetryService.stop();
		_storageService.stop();
		_securityService.stop();
		return false;
	}
	if (!_eventBus.start()) {
		_rfidService.stop();
		_sim7600.stop();
		_authenticationService.stop();
		_diagnosticsService.stop();
		return false;
	}
	_running = true;
	return true;
}

void Application::update()
{
	if (!_running) return;
	_rfidService.update();
	_alertManager.update();
	_alertManager.process();
	_powerMonitor.update();
	_powerManager.update();
	_diagnosticsService.update();
	_telemetryService.update();
}

void Application::stop()
{
	_rfidService.stop();
	_sim7600.stop();
	_authenticationService.stop();
	_diagnosticsService.stop();
	_telemetryService.stop();
	_storageService.stop();
	_securityService.stop();
	_running = false;
}

}  // namespace app
}  // namespace fortico
