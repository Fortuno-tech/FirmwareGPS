#include "managers/telemetry_manager/telemetry_manager.h"

#include "constants/events.h"
#include "utils/timer_utils.h"

namespace fortico {
namespace managers {

TelemetryManager::TelemetryManager(drivers::BatteryDriver& battery,
								   interfaces::IPublisher* publisher)
	: _battery(battery), _publisher(publisher), _packet{}, _initialized(false) {}

void TelemetryManager::init()
{
	_initialized = _battery.init() == constants::ErrorCode::OK ||
				   _battery.isInitialized();
	_packet = {};
	_packet.seq = 0U;
	_packet.timestamp = utils::Timer::millis();
}

void TelemetryManager::update()
{
	if (!_initialized) return;
	_battery.poll();
	float voltage = 0.0F;
	if (_battery.readVoltage(voltage)) {
		_packet.battery.voltage = voltage;
		_packet.battery.percentage = _battery.readPercentage();
		_packet.battery.isCritical = _battery.isCritical();
	}
	_packet.timestamp = utils::Timer::millis();
	++_packet.seq;
	if (_publisher != nullptr) {
		_publisher->publish({constants::EventID::SYSTEM_SELFTEST_PASSED,
							 _packet.timestamp, _packet.seq});
	}
}

void TelemetryManager::process() { update(); }
void TelemetryManager::reset() { _packet = {}; }
void TelemetryManager::setFuel(const types::FuelLevel& fuel) { _packet.fuel = fuel; }
void TelemetryManager::setGps(const types::GpsData& gps) { _packet.gps = gps; }
void TelemetryManager::setRelay(types::RelayState relay) { _packet.relay = relay; }
void TelemetryManager::setAuthentication(types::AuthenticationResult result) { _packet.lastAuthResult = result; }
void TelemetryManager::setTemperature(float temperatureC) { _packet.temperatureC = temperatureC; }
types::TelemetryPacket TelemetryManager::packet() const { return _packet; }

}  // namespace managers
}  // namespace fortico
