#include "drivers/sim7600/sim7600_driver.h"

#include <cstring>

#include "config/hardware_config.h"
#include "config/pin_config.h"
#include "utils/timer_utils.h"

namespace fortico {
namespace drivers {

Sim7600Driver::Sim7600Driver(hal::UartHal& uartHal)
	: _uartHal(uartHal), _initialized(false), _healthy(false), _active(false)
{
}

constants::ErrorCode Sim7600Driver::init()
{
	if (_initialized) return constants::ErrorCode::OK;
#ifdef ESP32_TARGET
	if (!_uartHal.init(1U, hardware::SIM7600_UART_BAUD_RATE,
					   pins::SIM7600_TX1, pins::SIM7600_RX1)) {
		return constants::ErrorCode::NETWORK_FAILURE;
	}
	_initialized = true;
	_active = true;
	if (!testAt(hardware::SIM7600_AT_TIMEOUT_MS)) {
		_healthy = false;
		return constants::ErrorCode::MODEM_AT_TIMEOUT;
	}
	_healthy = true;
	return constants::ErrorCode::OK;
#else
	return constants::ErrorCode::NETWORK_FAILURE;
#endif
}

constants::ErrorCode Sim7600Driver::poll()
{
	if (!_initialized) return constants::ErrorCode::NOT_INITIALIZED;
	if (!_active) return constants::ErrorCode::OK;
	return _healthy ? constants::ErrorCode::OK
					: constants::ErrorCode::MODEM_AT_TIMEOUT;
}

bool Sim7600Driver::start()
{
	if (!_initialized) return false;
	_active = true;
	return _healthy;
}

bool Sim7600Driver::stop()
{
	if (!_initialized) return false;
	_active = false;
	return true;
}

bool Sim7600Driver::isHealthy() const { return _initialized && _healthy; }
bool Sim7600Driver::isInitialized() const { return _initialized; }

void Sim7600Driver::reset()
{
	if (!_initialized) {
		_healthy = false;
		_active = false;
		return;
	}
	_healthy = testAt(hardware::SIM7600_AT_TIMEOUT_MS);
	_active = true;
}

bool Sim7600Driver::sendCommand(const char* command, char* response,
								std::size_t responseSize, uint32_t timeoutMs)
{
	if (!_initialized || !_active || command == nullptr || response == nullptr ||
		responseSize < 2U) {
		return false;
	}

	response[0] = '\0';
	const std::size_t commandLength = std::strlen(command);
	if (_uartHal.write(reinterpret_cast<const uint8_t*>(command),
					   commandLength) != commandLength ||
		_uartHal.write(reinterpret_cast<const uint8_t*>("\r\n"), 2U) != 2U) {
		_healthy = false;
		return false;
	}

	std::size_t length = 0U;
	    const uint32_t startMs = utils::Timer::millis();
	    while (utils::Timer::millis() - startMs < timeoutMs &&
		    length + 1U < responseSize) {
		uint8_t byte = 0U;
		if (_uartHal.read(&byte, 1U, 1U) == 0U) continue;
		response[length++] = static_cast<char>(byte);
		response[length] = '\0';
		if (std::strstr(response, "\r\nOK") != nullptr ||
			std::strstr(response, "\nOK") != nullptr) {
			return true;
		}
		if (std::strstr(response, "\r\nERROR") != nullptr ||
			std::strstr(response, "\nERROR") != nullptr) {
			return false;
		}
	}
	return false;
}

bool Sim7600Driver::writeRaw(const uint8_t* data, std::size_t length,
							 char* response, std::size_t responseSize,
							 uint32_t timeoutMs)
{
	if (!_initialized || !_active || data == nullptr || length == 0U ||
		response == nullptr || responseSize < 2U) return false;
	response[0] = '\0';
	if (_uartHal.write(data, length) != length) return false;
	std::size_t received = 0U;
	const uint32_t startMs = utils::Timer::millis();
	while (utils::Timer::millis() - startMs < timeoutMs &&
		   received + 1U < responseSize) {
		uint8_t byte = 0U;
		if (_uartHal.read(&byte, 1U, 1U) == 0U) continue;
		response[received++] = static_cast<char>(byte);
		response[received] = '\0';
		if (std::strstr(response, "OK") != nullptr) return true;
		if (std::strstr(response, "ERROR") != nullptr) return false;
	}
	return false;
}

bool Sim7600Driver::testAt(uint32_t timeoutMs)
{
	char response[96]{};
	return sendCommand("AT", response, sizeof(response), timeoutMs);
}

}  // namespace drivers
}  // namespace fortico
