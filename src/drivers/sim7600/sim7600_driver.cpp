#include "drivers/sim7600/sim7600_driver.h"

#include <cstring>
#include <cstdio>

#include "config/hardware_config.h"
#include "config/pin_config.h"
#include "utils/timer_utils.h"

namespace fortico {
namespace drivers {

Sim7600Driver::Sim7600Driver(hal::UartHal& uartHal, hal::DelayHal& delayHal)
	: _uartHal(uartHal), _delayHal(delayHal), _initialized(false), _healthy(false), _active(false),
	  _dataConnected(false)
{
}

constants::ErrorCode Sim7600Driver::init()
{
#ifdef ESP32_TARGET
	if (!_initialized) {
		if (!_uartHal.init(1U, hardware::SIM7600_UART_BAUD_RATE,
						   pins::SIM7600_TX1, pins::SIM7600_RX1)) {
			return constants::ErrorCode::NETWORK_FAILURE;
		}
		_initialized = true;
	}
	_active = true;
	for (uint8_t attempt = 0U;
		 attempt < hardware::SIM7600_AT_MAX_ATTEMPTS; ++attempt) {
		if (testAt(hardware::SIM7600_AT_TIMEOUT_MS)) {
			_healthy = true;
			return constants::ErrorCode::OK;
		}
		if (attempt + 1U < hardware::SIM7600_AT_MAX_ATTEMPTS) {
			_delayHal.waitMs(hardware::SIM7600_AT_RETRY_DELAY_MS);
		}
	}
	_healthy = false;
	return constants::ErrorCode::MODEM_AT_TIMEOUT;
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
	_dataConnected = false;
}

bool Sim7600Driver::sendCommand(const char* command, char* response,
								std::size_t responseSize, uint32_t timeoutMs)
{
	return sendCommandUntil(command, "\nOK", response, responseSize, timeoutMs);
}

bool Sim7600Driver::sendMqttCommand(const char* command,
								const char* successfulUrc, char* response,
								std::size_t responseSize, uint32_t timeoutMs)
{
	return sendCommandUntil(command, successfulUrc, response, responseSize,
							timeoutMs);
}

bool Sim7600Driver::beginMqttInput(const char* command, char* response,
								  std::size_t responseSize, uint32_t timeoutMs)
{
	return sendCommandUntil(command, ">", response, responseSize, timeoutMs);
}

bool Sim7600Driver::sendCommandUntil(const char* command,
								  const char* completionToken, char* response,
								  std::size_t responseSize, uint32_t timeoutMs)
{
	if (!_initialized || !_active || command == nullptr || response == nullptr ||
		responseSize < 2U || completionToken == nullptr || completionToken[0] == '\0') {
		return false;
	}

	drainInput();
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
		if (_uartHal.read(&byte, 1U, 1U) == 0U) {
			_delayHal.waitMs(1U);
			continue;
		}
		response[length++] = static_cast<char>(byte);
		response[length] = '\0';
		if (std::strstr(response, completionToken) != nullptr) {
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
		if (_uartHal.read(&byte, 1U, 1U) == 0U) {
			_delayHal.waitMs(1U);
			continue;
		}
		response[received++] = static_cast<char>(byte);
		response[received] = '\0';
		if (std::strstr(response, "\nOK") != nullptr) return true;
		if (std::strstr(response, "\r\nERROR") != nullptr ||
			std::strstr(response, "\nERROR") != nullptr) return false;
	}
	return false;
}

bool Sim7600Driver::testAt(uint32_t timeoutMs)
{
	char response[96]{};
	return sendCommand("AT", response, sizeof(response), timeoutMs);
}

bool Sim7600Driver::getModemInformation(char* information,
								 std::size_t informationSize)
{
	return sendDiagnosticCommand("ATI", information, informationSize,
							 hardware::SIM7600_AT_TIMEOUT_LONG_MS) &&
		(std::strstr(information, "SIMCOM") != nullptr ||
		 std::strstr(information, "SIM7600") != nullptr);
}

bool Sim7600Driver::isSimReady()
{
	char response[96]{};
	return sendDiagnosticCommand("AT+CPIN?", response, sizeof(response),
							 hardware::SIM7600_AT_TIMEOUT_MS) &&
		std::strstr(response, "+CPIN: READY") != nullptr;
}

bool Sim7600Driver::hasSignal()
{
	char response[96]{};
	return sendDiagnosticCommand("AT+CSQ", response, sizeof(response),
							 hardware::SIM7600_AT_TIMEOUT_MS) &&
		std::strstr(response, "+CSQ:") != nullptr &&
		std::strstr(response, "+CSQ: 99") == nullptr;
}

bool Sim7600Driver::isNetworkRegistered()
{
	char response[96]{};
	return sendDiagnosticCommand("AT+CREG?", response, sizeof(response),
							 hardware::SIM7600_AT_TIMEOUT_MS) &&
		(std::strstr(response, ",1") != nullptr ||
		 std::strstr(response, ",5") != nullptr);
}

bool Sim7600Driver::hasOperator()
{
	char response[128]{};
	return sendDiagnosticCommand("AT+COPS?", response, sizeof(response),
							 hardware::SIM7600_AT_TIMEOUT_LONG_MS) &&
		std::strstr(response, "+COPS:") != nullptr;
}

bool Sim7600Driver::hasRadioService()
{
	char response[160]{};
	return sendDiagnosticCommand("AT+CPSI?", response, sizeof(response),
							 hardware::SIM7600_AT_TIMEOUT_LONG_MS) &&
		std::strstr(response, "+CPSI:") != nullptr &&
		std::strstr(response, "NO SERVICE") == nullptr;
}

bool Sim7600Driver::isPacketAttached()
{
	char response[96]{};
	return sendDiagnosticCommand("AT+CGATT?", response, sizeof(response),
							 hardware::SIM7600_AT_TIMEOUT_MS) &&
		std::strstr(response, "+CGATT: 1") != nullptr;
}

bool Sim7600Driver::waitForNetwork(uint32_t timeoutMs)
{
	if (!_initialized || !_active || timeoutMs == 0U) return false;
	const uint32_t startMs = utils::Timer::millis();
	while (utils::Timer::millis() - startMs < timeoutMs) {
		if (isNetworkRegistered() && hasRadioService()) {
			return true;
		}
		_delayHal.waitMs(1000U);
	}
	return false;
}

bool Sim7600Driver::configurePdp(const char* apn)
{
	if (!_initialized || !_active || apn == nullptr || apn[0] == '\0' ||
		std::strlen(apn) > 96U) return false;
	char command[128]{};
	char response[128]{};
	std::snprintf(command, sizeof(command),
		"AT+CGDCONT=1,\"IP\",\"%s\"", apn);
	const bool configured = sendCommand(command, response, sizeof(response),
		hardware::SIM7600_AT_TIMEOUT_LONG_MS);
	if (configured) _dataConnected = false;
	return configured;
}

bool Sim7600Driver::activatePdp(uint32_t timeoutMs)
{
	if (!_initialized || !_active || timeoutMs == 0U) return false;
	char response[128]{};
	if (!sendCommand("AT+CGATT=1", response, sizeof(response), timeoutMs)) {
		return false;
	}
	_dataConnected = sendCommand("AT+CGACT=1,1", response, sizeof(response),
								 timeoutMs);
	return _dataConnected;
}

bool Sim7600Driver::getIpAddress(char* ipAddress, std::size_t ipAddressSize)
{
	if (ipAddress == nullptr || ipAddressSize < 2U) return false;
	char response[128]{};
	ipAddress[0] = '\0';
	if (!sendDiagnosticCommand("AT+CGPADDR=1", response, sizeof(response),
							hardware::SIM7600_AT_TIMEOUT_LONG_MS)) {
		return false;
	}
	const char* prefix = std::strstr(response, "+CGPADDR:");
	if (prefix == nullptr) return false;
	const char* comma = std::strchr(prefix, ',');
	if (comma == nullptr) return false;
	++comma;
	while (*comma == ' ' || *comma == '\"') ++comma;
	std::size_t length = 0U;
	while (comma[length] != '\0' && comma[length] != '\r' &&
		   comma[length] != '\n' && comma[length] != '\"' &&
		   length + 1U < ipAddressSize) {
		ipAddress[length] = comma[length];
		++length;
	}
	ipAddress[length] = '\0';
	return length > 0U && std::strcmp(ipAddress, "0.0.0.0") != 0;
}

bool Sim7600Driver::isDataConnected() const
{
	return _initialized && _active && _dataConnected;
}

bool Sim7600Driver::hasCertificate(const char* certificateFile)
{
	if (certificateFile == nullptr || certificateFile[0] == '\0') return false;
	char response[512]{};
	return sendCommand("AT+CCERTLIST", response, sizeof(response),
					   hardware::SIM7600_AT_TIMEOUT_LONG_MS) &&
		std::strstr(response, certificateFile) != nullptr;
}

bool Sim7600Driver::deleteCertificate(const char* certificateFile)
{
	if (certificateFile == nullptr || certificateFile[0] == '\0') return false;
	if (!_initialized || !_active) return false;
	char command[64]{};
	char response[128]{};
	std::snprintf(command, sizeof(command), "AT+CCERTDELE=\"%s\"", certificateFile);
	// Per specification: ignore error if the certificate does not exist
	sendCommand(command, response, sizeof(response), hardware::SIM7600_AT_TIMEOUT_MS);
	return true;
}

bool Sim7600Driver::downloadCertificate(const char* certificateFile,
										const uint8_t* certData,
										std::size_t certLen)
{
	if (certificateFile == nullptr || certificateFile[0] == '\0' ||
		certData == nullptr || certLen == 0U || certLen > 10240U) {
		return false;
	}
	if (!_initialized || !_active) return false;

	char command[64]{};
	char promptResponse[64]{};
	std::snprintf(command, sizeof(command), "AT+CCERTDOWN=\"%s\",%u",
				  certificateFile, static_cast<unsigned int>(certLen));

	if (!sendCommandUntil(command, ">", promptResponse, sizeof(promptResponse),
						  hardware::SIM7600_AT_TIMEOUT_LONG_MS)) {
		return false;
	}

	std::size_t totalWritten = 0U;
	while (totalWritten < certLen) {
		std::size_t chunk = certLen - totalWritten;
		if (chunk > 256U) chunk = 256U;
		std::size_t n = _uartHal.write(certData + totalWritten, chunk);
		if (n == 0U) {
			_delayHal.waitMs(2U);
			n = _uartHal.write(certData + totalWritten, chunk);
			if (n == 0U) return false;
		}
		totalWritten += n;
	}

	// Wait for confirmation: \nOK, \r\nOK (timeout up to 15s)
	char resp[256]{};
	std::size_t respLen = 0U;
	const uint32_t startMs = utils::Timer::millis();
	bool confirmed = false;
	while (utils::Timer::millis() - startMs < 15000U && respLen + 1U < sizeof(resp)) {
		uint8_t byte = 0U;
		if (_uartHal.read(&byte, 1U, 1U) == 0U) {
			_delayHal.waitMs(2U);
			continue;
		}
		resp[respLen++] = static_cast<char>(byte);
		resp[respLen] = '\0';
		if (std::strstr(resp, "\nOK") != nullptr ||
			std::strstr(resp, "\r\nOK") != nullptr) {
			confirmed = true;
			break;
		}
		if (std::strstr(resp, "\r\nERROR") != nullptr ||
			std::strstr(resp, "\nERROR") != nullptr) {
			return false;
		}
	}

	if (!confirmed) return false;

	// Verify certificate is present in AT+CCERTLIST
	return hasCertificate(certificateFile);
}

bool Sim7600Driver::sendDiagnosticCommand(const char* command, char* response,
											 std::size_t responseSize, uint32_t timeoutMs)
{
	return sendCommand(command, response, responseSize, timeoutMs);
}

void Sim7600Driver::drainInput()
{
	uint8_t discarded = 0U;
	while (_uartHal.available() > 0) {
		if (_uartHal.read(&discarded, 1U, 0U) == 0U) break;
	}
}

}  // namespace drivers
}  // namespace fortico
