#include "app/application/application.h"

namespace fortico {
namespace app {

Application::Application(core::EventBus& eventBus,
                         managers::AlertManager& alertManager,
                         services::AuthenticationService& authenticationService,
                         services::RfidService& rfidService,
                         services::Sim7600Service& sim7600Service,
                         services::GpsService& gpsService,
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
                         middleware::CellularMqttClient& mqttClient)
    : _systemController(eventBus, alertManager, authenticationService,
                        rfidService, sim7600Service, gpsService, diagnosticsService,
                        powerMonitor,
                        errorHandler, securityService, storageService,
                        telemetryService, powerManager, otaManager,
                        relayDriver, relayManager, fuelDriver, fuelManager,
                        rtcDriver, sdDriver, mqttClient),
      _taskManager(), _stateMachine(), _initialized(false), _running(false)
{
}

bool Application::init()
{
    if (_initialized) return true;
    if (!_stateMachine.start() || !_stateMachine.enterInitialization()) return false;
    if (!_systemController.init()) {
        _stateMachine.enterError();
        return false;
    }
    _initialized = true;
    return true;
}

bool Application::start()
{
    if (!_initialized || _running) return _running;
    _stateMachine.enterNetwork();
    if (!_systemController.start()) {
        _stateMachine.enterError();
        return false;
    }
    if (!_taskManager.addPeriodic(100U, &Application::updateTask, this) ||
        !_taskManager.start()) {
        _systemController.stop();
        _stateMachine.enterError();
        return false;
    }
    _stateMachine.enterOperational();
    _running = true;
    return true;
}

void Application::update()
{
    if (_running) {
        _stateMachine.update(_systemController.powerState(),
                             _systemController.hasFault());
    }
}

void Application::stop()
{
    _taskManager.stop();
    _systemController.stop();
    _running = false;
}

void Application::updateTask(void* context)
{
    static_cast<Application*>(context)->scheduledUpdate();
}

void Application::scheduledUpdate()
{
    _systemController.update();
    _stateMachine.update(_systemController.powerState(),
                         _systemController.hasFault());
}

types::SystemState Application::state() const { return _stateMachine.state(); }

}  // namespace app
}  // namespace fortico
