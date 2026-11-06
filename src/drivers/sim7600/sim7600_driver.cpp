#include "drivers/sim7600/sim7600_driver.h"

#include <cstring>
#include <cstdio>
#include <cctype>
#ifdef ESP32_TARGET
#include <Arduino.h>
#endif

#include "config/hardware_config.h"
#include "config/pin_config.h"
#include "utils/timer_utils.h"

namespace fortico {
namespace drivers {

Sim7600Driver::Sim7600Driver(hal::UartHal& uartHal, hal::DelayHal& delayHal)
	: _uartHal(uartHal), _delayHal(delayHal), _initialized(false), _healthy(false), _active(false),
	  _dataConnected(false), _lastCommandFailure(CommandFailure::NONE)
#ifdef ESP32_TARGET
	  , _transactionMutex(xSemaphoreCreateRecursiveMutex())
#endif
{
	resetIncomingParser();
}

namespace {
enum IncomingParserState : uint8_t {
	RX_IDLE, RX_TOPIC_BODY, RX_PAYLOAD_BODY, RX_WAIT_END
};
}

bool Sim7600Driver::lockTransaction(uint32_t timeoutMs)
{
#ifdef ESP32_TARGET
	if (_transactionMutex == nullptr) return false;
	const TickType_t ticks = timeoutMs == portMAX_DELAY
		? portMAX_DELAY : pdMS_TO_TICKS(timeoutMs);
	return xSemaphoreTakeRecursive(_transactionMutex, ticks) == pdTRUE;
#else
	(void)timeoutMs;
	return true;
#endif
}

void Sim7600Driver::unlockTransaction()
{
#ifdef ESP32_TARGET
	if (_transactionMutex != nullptr) xSemaphoreGiveRecursive(_transactionMutex);
#endif
}

Sim7600Driver::CommandFailure Sim7600Driver::lastCommandFailure() const
{
	return _lastCommandFailure;
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
	_lastCommandFailure = CommandFailure::NONE;
	if (!_initialized || !_active || command == nullptr || response == nullptr ||
		responseSize < 2U || completionToken == nullptr || completionToken[0] == '\0') {
		_lastCommandFailure = CommandFailure::INVALID;
		return false;
	}
	if (!lockTransaction(hardware::SIM7600_UART_MUTEX_TIMEOUT_MS)) {
		_lastCommandFailure = CommandFailure::MUTEX_TIMEOUT;
		return false;
	}

	drainInput();
	response[0] = '\0';
	const std::size_t commandLength = std::strlen(command);
	if (_uartHal.write(reinterpret_cast<const uint8_t*>(command),
					   commandLength) != commandLength ||
		_uartHal.write(reinterpret_cast<const uint8_t*>("\r\n"), 2U) != 2U) {
		_lastCommandFailure = CommandFailure::UART_WRITE;
		_healthy = false;
		unlockTransaction();
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
		consumeIncomingByte(byte);
		const char* cmqttsub = std::strstr(response, "+CMQTTSUB:");
		if (cmqttsub != nullptr) {
			unsigned int client = 0U;
			unsigned int err = 0U;
			if (std::sscanf(cmqttsub, "+CMQTTSUB: %u,%u", &client, &err) == 2) {
				if (err != 0U) {
					_lastCommandFailure = CommandFailure::ERROR_RESPONSE;
					unlockTransaction();
					return false;
				}
			}
		}
		if (std::strstr(response, completionToken) != nullptr) {
			unlockTransaction();
			return true;
		}
		if (std::strstr(response, "\r\nERROR") != nullptr ||
			std::strstr(response, "\nERROR") != nullptr) {
			_lastCommandFailure = CommandFailure::ERROR_RESPONSE;
			unlockTransaction();
			return false;
		}
	}
	_lastCommandFailure = CommandFailure::TIMEOUT;
	unlockTransaction();
	return false;
}

bool Sim7600Driver::writeRaw(const uint8_t* data, std::size_t length,
							 char* response, std::size_t responseSize,
							 uint32_t timeoutMs)
{
	if (!_initialized || !_active || data == nullptr || length == 0U ||
		response == nullptr || responseSize < 2U) return false;
	if (!lockTransaction(hardware::SIM7600_UART_MUTEX_TIMEOUT_MS)) return false;
	response[0] = '\0';
	if (_uartHal.write(data, length) != length) {
		unlockTransaction();
		return false;
	}
	std::size_t received = 0U;
	char recent[8]{};
	std::size_t recentLength = 0U;
	const uint32_t startMs = utils::Timer::millis();
	while (utils::Timer::millis() - startMs < timeoutMs) {
		uint8_t byte = 0U;
		if (_uartHal.read(&byte, 1U, 1U) == 0U) {
			_delayHal.waitMs(1U);
			continue;
		}
		if (received + 1U < responseSize) {
			response[received++] = static_cast<char>(byte);
			response[received] = '\0';
		}
		consumeIncomingByte(byte);
		if (recentLength + 1U >= sizeof(recent)) {
			std::memmove(recent, recent + 1U, sizeof(recent) - 2U);
			recentLength = sizeof(recent) - 2U;
		}
		recent[recentLength++] = static_cast<char>(byte);
		recent[recentLength] = '\0';
		if (std::strstr(recent, "\r\nOK\r\n") != nullptr ||
			std::strstr(recent, "\nOK\n") != nullptr) {
			unlockTransaction();
			return true;
		}
		if (std::strstr(recent, "\r\nERROR") != nullptr ||
			std::strstr(recent, "\nERROR") != nullptr) {
			unlockTransaction();
			return false;
		}
	}
	unlockTransaction();
	return false;
}

bool Sim7600Driver::pollMqttIncoming(MqttIncomingMessage& message)
{
	if (takeIncomingMessage(message)) return true;
	if (!_initialized || !_active || !lockTransaction(0U)) return false;
	uint8_t byte = 0U;
	while (_uartHal.available() > 0 && _uartHal.read(&byte, 1U, 0U) == 1U) {
		consumeIncomingByte(byte);
		if (takeIncomingMessage(message)) {
			unlockTransaction();
			return true;
		}
	}
	unlockTransaction();
	return takeIncomingMessage(message);
}

void Sim7600Driver::resetIncomingParser()
{
	_incomingParserState = RX_IDLE;
	_incomingExpected = 0U;
	_incomingPosition = 0U;
	_incomingTopicTotal = 0U;
	_incomingPayloadTotal = 0U;
	_incomingLastByteMs = 0U;
	_incomingLineLength = 0U;
	_incomingMessageReady = false;
	_incomingConnectionLost = false;
	std::memset(&_incomingMessage, 0, sizeof(_incomingMessage));
	std::memset(_incomingLine, 0, sizeof(_incomingLine));
}

bool Sim7600Driver::takeIncomingMessage(MqttIncomingMessage& message)
{
	if (!_incomingMessageReady) return false;
	message = _incomingMessage;
	_incomingMessageReady = false;
	resetIncomingParser();
	return true;
}

bool Sim7600Driver::takeMqttConnectionLost()
{
	const bool lost = _incomingConnectionLost;
	_incomingConnectionLost = false;
	return lost;
}

void Sim7600Driver::consumeIncomingByte(uint8_t byte)
{
	if (_incomingMessageReady) return;
	const uint32_t nowMs = utils::Timer::millis();
	if ((_incomingParserState == RX_TOPIC_BODY ||
		 _incomingParserState == RX_PAYLOAD_BODY ||
		 _incomingParserState == RX_WAIT_END) &&
		nowMs - _incomingLastByteMs > 5000U) {
		resetIncomingParser();
	}
	_incomingLastByteMs = nowMs;
	if (_incomingParserState == RX_TOPIC_BODY ||
		_incomingParserState == RX_PAYLOAD_BODY) {
		if (_incomingPosition < _incomingExpected) {
			if (_incomingParserState == RX_TOPIC_BODY &&
				_incomingPosition + 1U < sizeof(_incomingMessage.topic)) {
				_incomingMessage.topic[_incomingPosition] = static_cast<char>(byte);
			} else if (_incomingParserState == RX_PAYLOAD_BODY &&
				_incomingPosition + 1U < sizeof(_incomingMessage.payload)) {
				_incomingMessage.payload[_incomingPosition] = static_cast<char>(byte);
			}
			++_incomingPosition;
		}
		if (_incomingPosition == _incomingExpected) {
			if (_incomingParserState == RX_TOPIC_BODY) {
				_incomingMessage.topicLength = _incomingExpected;
				_incomingMessage.topic[_incomingExpected] = '\0';
				_incomingParserState = RX_IDLE;
			} else {
				_incomingMessage.payloadLength = _incomingExpected;
				_incomingMessage.payload[_incomingExpected] = '\0';
				_incomingParserState = RX_WAIT_END;
			}
		}
		return;
	}
	if (byte == '\n') {
		_incomingLine[_incomingLineLength] = '\0';
		unsigned int client = 0U;
		unsigned int totalTopic = 0U;
		unsigned int totalPayload = 0U;
		unsigned int length = 0U;
		unsigned int endClient = 0U;
		if (std::sscanf(_incomingLine, "+CMQTTRXSTART: %u,%u,%u",
			&client, &totalTopic, &totalPayload) == 3 &&
			totalTopic < sizeof(_incomingMessage.topic) &&
			totalPayload < sizeof(_incomingMessage.payload)) {
			_incomingMessage.clientIndex = static_cast<uint8_t>(client);
			_incomingMessage.topicLength = 0U;
			_incomingMessage.payloadLength = 0U;
			_incomingTopicTotal = totalTopic;
			_incomingPayloadTotal = totalPayload;
		} else if (std::sscanf(_incomingLine, "+CMQTTRXTOPIC: %u,%u",
			&client, &length) == 2 && length < sizeof(_incomingMessage.topic) &&
			length <= _incomingTopicTotal) {
			_incomingMessage.clientIndex = static_cast<uint8_t>(client);
			_incomingExpected = length;
			_incomingPosition = 0U;
			_incomingParserState = RX_TOPIC_BODY;
		} else if (std::sscanf(_incomingLine, "+CMQTTRXPAYLOAD: %u,%u",
			&client, &length) == 2 && length < sizeof(_incomingMessage.payload) &&
			length <= _incomingPayloadTotal) {
			_incomingMessage.clientIndex = static_cast<uint8_t>(client);
			_incomingExpected = length;
			_incomingPosition = 0U;
			_incomingParserState = RX_PAYLOAD_BODY;
		} else if (std::sscanf(_incomingLine, "+CMQTTRXEND: %u", &endClient) == 1 &&
			_incomingParserState == RX_WAIT_END &&
			endClient == _incomingMessage.clientIndex &&
			_incomingMessage.topicLength == _incomingTopicTotal &&
			_incomingMessage.payloadLength == _incomingPayloadTotal) {
			_incomingMessageReady = true;
		} else if (std::strstr(_incomingLine, "+CMQTTCONNLOST:") != nullptr ||
			std::strstr(_incomingLine, "+CMQTTNONET") != nullptr) {
			resetIncomingParser();
			_incomingConnectionLost = true;
		}
		_incomingLineLength = 0U;
		return;
	}
	if (byte != '\r' && _incomingLineLength + 1U < sizeof(_incomingLine)) {
		_incomingLine[_incomingLineLength++] = static_cast<char>(byte);
	} else if (_incomingLineLength + 1U >= sizeof(_incomingLine)) {
		resetIncomingParser();
	}
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

bool Sim7600Driver::getNetworkDateTime(RtcDateTime& dateTime)
{
	if (!_initialized || !_active || !_healthy) return false;
	char response[128]{};
	if (!sendCommand("AT+CCLK?", response, sizeof(response),
					hardware::SIM7600_AT_TIMEOUT_MS)) {
		return false;
	}
	unsigned int year = 0U;
	unsigned int month = 0U;
	unsigned int day = 0U;
	unsigned int hour = 0U;
	unsigned int minute = 0U;
	unsigned int second = 0U;
	const char* timeText = std::strchr(response, '"');
	char timezoneSign = '\0';
	unsigned int timezoneQuarters = 0U;
	if (timeText == nullptr) return false;
	const int fieldsWithTimezone = std::sscanf(
		timeText + 1, "%u/%u/%u,%u:%u:%u%c%u",
		&year, &month, &day, &hour, &minute, &second,
		&timezoneSign, &timezoneQuarters);
	if (fieldsWithTimezone != 8) {
		const int fieldsWithoutTimezone = std::sscanf(
			timeText + 1, "%u/%u/%u,%u:%u:%u",
			&year, &month, &day, &hour, &minute, &second);
		if (fieldsWithoutTimezone != 6) return false;
		timezoneSign = '+';
		timezoneQuarters = 0U;
	} else if ((timezoneSign != '+' && timezoneSign != '-') || timezoneQuarters > 96U) {
		return false;
	}
	RtcDateTime value{
		static_cast<uint16_t>(2000U + year), static_cast<uint8_t>(month),
		static_cast<uint8_t>(day), static_cast<uint8_t>(hour),
		static_cast<uint8_t>(minute), static_cast<uint8_t>(second)};
	if (value.year < 2000U || value.year > 2099U || value.month < 1U ||
		value.month > 12U || value.day < 1U || value.day > 31U ||
		value.hour > 23U || value.minute > 59U || value.second > 59U) {
		return false;
	}
	int offsetMinutes = static_cast<int>(timezoneQuarters) * 15;
	if (timezoneSign == '-') offsetMinutes = -offsetMinutes;
	int totalMinutes = static_cast<int>(value.hour) * 60 +
		static_cast<int>(value.minute) - offsetMinutes;
	while (totalMinutes < 0) {
		totalMinutes += 1440;
		if (value.day > 1U) {
			--value.day;
		} else {
			if (value.month == 1U) { value.month = 12U; --value.year; }
			else { --value.month; }
			const bool leap = (value.year % 4U == 0U && value.year % 100U != 0U) ||
				(value.year % 400U == 0U);
			static constexpr uint8_t days[] =
				{31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U};
			value.day = days[value.month - 1U] +
				((value.month == 2U && leap) ? 1U : 0U);
		}
	}
	while (totalMinutes >= 1440) {
		totalMinutes -= 1440;
		static constexpr uint8_t days[] =
			{31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U};
		const bool leap = (value.year % 4U == 0U && value.year % 100U != 0U) ||
			(value.year % 400U == 0U);
		const uint8_t monthDays = days[value.month - 1U] +
			((value.month == 2U && leap) ? 1U : 0U);
		if (value.day < monthDays) ++value.day;
		else { value.day = 1U; if (value.month == 12U) { value.month = 1U; ++value.year; } else ++value.month; }
	}
	value.hour = static_cast<uint8_t>(totalMinutes / 60);
	value.minute = static_cast<uint8_t>(totalMinutes % 60);
	dateTime = value;
	return true;
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

	// Disable modem echo so payload bytes are not echoed back over UART
	char echoResp[32]{};
	sendCommand("ATE0", echoResp, sizeof(echoResp), hardware::SIM7600_AT_TIMEOUT_MS);
	drainInput();

	char command[64]{};
	char promptResponse[64]{};
	std::snprintf(command, sizeof(command), "AT+CCERTDOWN=\"%s\",%u",
				  certificateFile, static_cast<unsigned int>(certLen));

	if (!sendCommandUntil(command, ">", promptResponse, sizeof(promptResponse),
						  hardware::SIM7600_AT_TIMEOUT_LONG_MS)) {
#ifdef ESP32_TARGET
		Serial.printf("[PROV] Prompt '>' not received: '%s'\n", promptResponse);
#endif
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
		// If the modem was echoing while transmitting, drain echoed bytes
		// so UART RX FIFO never overflows before transmission completes.
		if (totalWritten < certLen) {
			drainInput();
		}
	}
	_delayHal.waitMs(50U);

	// Wait for confirmation: \r\nOK\r\n (timeout up to 15s).
	// We read using a sliding window so that any remaining echoed payload bytes
	// are absorbed without buffer overflow or premature exit.
	char tail[128]{};
	std::size_t tailLen = 0U;
	std::size_t totalBytesReceived = 0U;
	const uint32_t startMs = utils::Timer::millis();
	bool confirmed = false;

	while (utils::Timer::millis() - startMs < 15000U) {
		uint8_t byte = 0U;
		if (_uartHal.read(&byte, 1U, 5U) == 0U) {
			_delayHal.waitMs(2U);
			continue;
		}
		totalBytesReceived++;

		if (tailLen + 1U < sizeof(tail)) {
			tail[tailLen++] = static_cast<char>(byte);
			tail[tailLen] = '\0';
		} else {
			std::memmove(tail, tail + 1, sizeof(tail) - 2);
			tail[sizeof(tail) - 2] = static_cast<char>(byte);
			tail[sizeof(tail) - 1] = '\0';
		}

		if (std::strstr(tail, "\r\nOK\r\n") != nullptr ||
			std::strstr(tail, "\nOK\r\n") != nullptr ||
			std::strstr(tail, "\r\nOK\n") != nullptr ||
			std::strstr(tail, "\nOK\n") != nullptr) {
			confirmed = true;
			break;
		}
		// Also accept OK followed by silence (end of transmission)
		if ((std::strstr(tail, "\r\nOK") != nullptr ||
			 std::strstr(tail, "\nOK") != nullptr) &&
			_uartHal.available() == 0) {
			_delayHal.waitMs(20U);
			if (_uartHal.available() == 0) {
				confirmed = true;
				break;
			}
		}

		if (std::strstr(tail, "\r\nERROR") != nullptr ||
			std::strstr(tail, "\nERROR") != nullptr ||
			std::strstr(tail, "+CME ERROR") != nullptr) {
#ifdef ESP32_TARGET
			Serial.printf("[PROV] SIM7600 returned error during CCERTDOWN: '%s'\n", tail);
#endif
			return false;
		}
	}

	if (!confirmed) {
#ifdef ESP32_TARGET
		Serial.printf("[PROV] SIM7600 confirm timeout (read %u bytes, last='%s')\n",
					  static_cast<unsigned int>(totalBytesReceived), tail);
#endif
		return false;
	}

	// Verify certificate is present in AT+CCERTLIST
	const bool present = hasCertificate(certificateFile);
#ifdef ESP32_TARGET
	if (!present) {
		Serial.println(F("[PROV] Certificate missing in AT+CCERTLIST after download"));
	} else {
		Serial.println(F("[PROV] Certificate verified in AT+CCERTLIST"));
	}
#endif
	return present;
}

bool Sim7600Driver::sendDiagnosticCommand(const char* command, char* response,
											 std::size_t responseSize, uint32_t timeoutMs)
{
	return sendCommand(command, response, responseSize, timeoutMs);
}

void Sim7600Driver::drainInput()
{
	uint8_t buffered = 0U;
	while (_uartHal.available() > 0) {
		if (_uartHal.read(&buffered, 1U, 0U) == 0U) break;
		consumeIncomingByte(buffered);
	}
}

}  // namespace drivers
}  // namespace fortico
