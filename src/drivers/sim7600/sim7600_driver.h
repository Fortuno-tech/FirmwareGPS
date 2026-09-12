#pragma once

#include <cstddef>
#include <cstdint>

#include "drivers/rtc/rtc_driver.h"
#include "constants/errors.h"
#include "hal/delay/delay_hal.h"
#include "hal/uart/uart_hal.h"
#include "interfaces/idriver.h"

#ifdef ESP32_TARGET
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

namespace fortico {
namespace drivers {

class Sim7600Driver : public interfaces::IDriver {
public:
	Sim7600Driver(hal::UartHal& uartHal, hal::DelayHal& delayHal);

	constants::ErrorCode init() override;
	constants::ErrorCode poll() override;
	bool start() override;
	bool stop() override;
	bool isHealthy() const override;
	bool isInitialized() const override;
	void reset() override;

	bool sendCommand(const char* command, char* response,
					 std::size_t responseSize, uint32_t timeoutMs);
	/// Attend l'URC final MQTT attendu ; un simple "OK" intermÃ©diaire ne suffit pas.
	bool sendMqttCommand(const char* command, const char* successfulUrc,
						 char* response, std::size_t responseSize,
						 uint32_t timeoutMs);
	/// Envoie une commande CMQTTxxx qui attend ensuite un topic/payload brut.
	bool beginMqttInput(const char* command, char* response,
					std::size_t responseSize, uint32_t timeoutMs);
	bool writeRaw(const uint8_t* data, std::size_t length,
				  char* response, std::size_t responseSize,
				  uint32_t timeoutMs);
	bool testAt(uint32_t timeoutMs = 1000U);
	bool getModemInformation(char* information, std::size_t informationSize);
	bool isSimReady();
	bool hasSignal();
	bool isNetworkRegistered();
	bool hasOperator();
	bool hasRadioService();
	bool isPacketAttached();
	bool waitForNetwork(uint32_t timeoutMs);
	bool configurePdp(const char* apn);
	bool activatePdp(uint32_t timeoutMs);
	bool getIpAddress(char* ipAddress, std::size_t ipAddressSize);
	bool isDataConnected() const;
	bool getNetworkDateTime(RtcDateTime& dateTime);
	bool hasCertificate(const char* certificateFile);
	bool deleteCertificate(const char* certificateFile);
	bool downloadCertificate(const char* certificateFile, const uint8_t* certData,
							 std::size_t certLen);
	bool lockTransaction(uint32_t timeoutMs);
	void unlockTransaction();

private:
	bool sendCommandUntil(const char* command, const char* completionToken,
						  char* response, std::size_t responseSize,
						  uint32_t timeoutMs);
	bool sendDiagnosticCommand(const char* command, char* response,
						   std::size_t responseSize, uint32_t timeoutMs);
	void drainInput();

	hal::UartHal& _uartHal;
	hal::DelayHal& _delayHal;
	bool _initialized;
	bool _healthy;
	bool _active;
	bool _dataConnected;
#ifdef ESP32_TARGET
	SemaphoreHandle_t _transactionMutex;
#endif
};

}  // namespace drivers
}  // namespace fortico
