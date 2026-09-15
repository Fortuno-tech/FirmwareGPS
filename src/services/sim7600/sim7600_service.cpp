#include "services/sim7600/sim7600_service.h"

#include "constants/errors.h"

namespace fortico {
namespace services {

Sim7600Service::Sim7600Service(drivers::Sim7600Driver& driver)
    : _driver(driver), _running(false)
{
}

bool Sim7600Service::start()
{
    if (_running) return true;
    if (_driver.init() != constants::ErrorCode::OK) return false;
    _running = _driver.start();
    return _running;
}

void Sim7600Service::stop()
{
    _driver.stop();
    _running = false;
}

bool Sim7600Service::isRunning() const
{
    return _running && _driver.isHealthy();
}

bool Sim7600Service::waitForNetwork(uint32_t timeoutMs)
{
    return isRunning() && _driver.waitForNetwork(timeoutMs);
}

bool Sim7600Service::connectInternet(const char* apn, uint32_t timeoutMs)
{
    if (!isRunning() || apn == nullptr || apn[0] == '\0') return false;
    if (!_driver.configurePdp(apn) || !_driver.activatePdp(timeoutMs)) return false;
    char ipAddress[48]{};
    return _driver.getIpAddress(ipAddress, sizeof(ipAddress));
}

bool Sim7600Service::runDiagnostics(Sim7600Diagnostics& diagnostics)
{
    diagnostics = {};
    if (!isRunning()) return false;
    // ATI may include manufacturer, model, revision, IMEI and capabilities.
    // 128 bytes can truncate a valid SIM7600 response before its final OK.
    char modemInformation[512]{};
    diagnostics.modemIdentified = _driver.getModemInformation(
        modemInformation, sizeof(modemInformation));
    diagnostics.simReady = _driver.isSimReady();
    diagnostics.signalAvailable = _driver.hasSignal();
    diagnostics.networkRegistered = _driver.isNetworkRegistered();
    diagnostics.operatorAvailable = _driver.hasOperator();
    diagnostics.radioServiceAvailable = _driver.hasRadioService();
    diagnostics.packetAttached = _driver.isPacketAttached();
    diagnostics.dataConnected = _driver.isDataConnected();
    return diagnostics.modemIdentified && diagnostics.simReady &&
           diagnostics.signalAvailable && diagnostics.networkRegistered &&
           diagnostics.radioServiceAvailable;
}

bool Sim7600Service::getIpAddress(char* ipAddress, std::size_t ipAddressSize)
{
    return isRunning() && _driver.getIpAddress(ipAddress, ipAddressSize);
}

bool Sim7600Service::isDataConnected() const
{
    return isRunning() && _driver.isDataConnected();
}

bool Sim7600Service::getNetworkDateTime(drivers::RtcDateTime& dateTime)
{
    return isDataConnected() && _driver.getNetworkDateTime(dateTime);
}

}  // namespace services
}  // namespace fortico
