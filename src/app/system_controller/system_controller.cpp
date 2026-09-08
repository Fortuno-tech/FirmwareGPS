#include "app/system_controller/system_controller.hpp"

#include "config/network_config.h"

namespace fortico {
namespace app {

SystemController::SystemController(
	core::EventBus& eventBus, managers::AlertManager& alertManager,
	services::AuthenticationService& authenticationService,
	services::RfidService& rfidService, drivers::Sim7600Driver& sim7600,
	services::DiagnosticsService& diagnosticsService,
	core::PowerMonitor& powerMonitor, core::ErrorHandler& errorHandler,
	services::SecurityService& securityService,
	services::StorageService& storageService,
	services::TelemetryService& telemetryService,
	managers::PowerManager& powerManager, managers::OtaManager& otaManager,
	drivers::RelayDriver& relayDriver, managers::RelayManager& relayManager,
	drivers::FuelDriver& fuelDriver, managers::FuelManager& fuelManager,
	drivers::RtcDriver& rtcDriver, drivers::SdDriver& sdDriver,
	middleware::CellularMqttClient& mqttClient)
	: _eventBus(eventBus), _alertManager(alertManager),
	  _authenticationService(authenticationService), _rfidService(rfidService),
	  _sim7600(sim7600), _diagnosticsService(diagnosticsService),
	  _powerMonitor(powerMonitor), _errorHandler(errorHandler),
	  _securityService(securityService), _storageService(storageService),
	  _telemetryService(telemetryService), _powerManager(powerManager),
	  _otaManager(otaManager), _relayDriver(relayDriver),
	  _relayManager(relayManager), _fuelDriver(fuelDriver),
	  _fuelManager(fuelManager), _rtcDriver(rtcDriver), _sdDriver(sdDriver),
	  _mqttClient(mqttClient),
	  _initialized(false), _running(false)
{
}

bool SystemController::init()
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
	_relayManager.init();
	if (_fuelDriver.init() != constants::ErrorCode::OK ||
		_rtcDriver.init() != constants::ErrorCode::OK) {
		_errorHandler.report(constants::ErrorCode::UNKNOWN);
		return false;
	}
	_sdDriver.init();
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

bool SystemController::start()
{
	if (!_initialized || _running) return _running;
	if (!_authenticationService.start()) return false;
	if (!_telemetryService.start()) {
		_authenticationService.stop();
		return false;
	}
	if (!_diagnosticsService.start()) {
		_authenticationService.stop();
		_telemetryService.stop();
		return false;
	}
	if (_sim7600.init() != constants::ErrorCode::OK || !_sim7600.start()) {
		_errorHandler.report(constants::ErrorCode::MODEM_AT_TIMEOUT);
		_authenticationService.stop();
		_diagnosticsService.stop();
		_telemetryService.stop();
		return false;
	}
	if (!_sim7600.waitForNetwork(network::MODEM_NETWORK_REGISTRATION_TIMEOUT_MS) ||
		!_sim7600.configurePdp(network::APN_DEFAULT) ||
		!_sim7600.activatePdp(network::MODEM_PDP_CONTEXT_TIMEOUT_MS)) {
		_errorHandler.report(constants::ErrorCode::MODEM_REGISTRATION_FAILED);
		_sim7600.stop();
		_authenticationService.stop();
		_diagnosticsService.stop();
		_telemetryService.stop();
		return false;
	}
	types::MqttCredentials credentials{};
	if (!_securityService.loadMqttCredentials(credentials) ||
		!_mqttClient.setCredentials(credentials) ||
		!_mqttClient.connect(network::MQTT_BROKER_HOST, network::MQTT_BROKER_PORT)) {
		_errorHandler.report(constants::ErrorCode::MQTT_CONNECT_FAILED);
		_sim7600.stop();
		_authenticationService.stop();
		_diagnosticsService.stop();
		_telemetryService.stop();
		return false;
	}
	if (!_rfidService.start()) {
		_mqttClient.disconnect();
		_sim7600.stop();
		_authenticationService.stop();
		_diagnosticsService.stop();
		_telemetryService.stop();
		return false;
	}
	if (!_eventBus.start()) {
		_rfidService.stop();
		_mqttClient.disconnect();
		_sim7600.stop();
		_authenticationService.stop();
		_diagnosticsService.stop();
		_telemetryService.stop();
		return false;
	}
	if (!_relayDriver.start() || !_fuelDriver.start() || !_rtcDriver.start()) {
		stop();
		return false;
	}
	_sdDriver.start();
	_running = true;
	return true;
}

void SystemController::update()
{
	if (!_running) return;
	_rfidService.update();
	_alertManager.update();
	_alertManager.process();
	_powerMonitor.update();
	_powerManager.update();
	_relayManager.update();
	_relayManager.process();
	_fuelManager.update();
	_fuelManager.process();
	_fuelDriver.poll();
	_rtcDriver.poll();
	_sdDriver.poll();
	_diagnosticsService.update();
	_telemetryService.update();
}

void SystemController::stop()
{
	_rfidService.stop();
	_mqttClient.disconnect();
	_sim7600.stop();
	_relayDriver.stop();
	_fuelDriver.stop();
	_rtcDriver.stop();
	_sdDriver.stop();
	_authenticationService.stop();
	_diagnosticsService.stop();
	_telemetryService.stop();
	_storageService.stop();
	_securityService.stop();
	_running = false;
}

bool SystemController::isInitialized() const { return _initialized; }
bool SystemController::isRunning() const { return _running; }
core::PowerState SystemController::powerState() const { return _powerMonitor.state(); }
bool SystemController::hasFault() const { return _errorHandler.hasError(); }

}  // namespace app
}  // namespace fortico
