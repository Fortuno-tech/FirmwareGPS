#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "app/connectivity_task/connectivity_request_mailbox.h"
#include "core/logger/logger.h"
#include "drivers/rtc/rtc_driver.h"
#include "drivers/sim7600/sim7600_driver.h"
#include "drivers/sim7600/sim7600_power_driver.h"
#include "middleware/mqtt_client/cellular_mqtt_client.h"
#include "services/gps/gps_service.h"
#include "services/security/security_service.h"
#include "services/sim7600/sim7600_service.h"

namespace fortico {
namespace app {

class ConnectivityTask {
public:
	ConnectivityTask(drivers::Sim7600PowerDriver& sim7600PowerDriver,
					 drivers::Sim7600Driver& sim7600Driver,
					 services::Sim7600Service& sim7600Service,
					 services::GpsService& gpsService,
					 services::SecurityService& securityService,
					 drivers::RtcDriver& rtcDriver,
					 middleware::CellularMqttClient& mqttClient,
					 core::Logger& logger);

	bool start(bool enableNetwork = true);
	void stop();
	void requestReconnect();
	bool networkConnected() const;
	bool mqttConnected() const;
	bool hasModemCertificate(const char* certificateName);
	bool installModemCertificate(const char* certificateName,
								 std::size_t expectedSize,
								 const char* base64Data);

private:
	enum class ProvisioningOperation : uint8_t {
		CheckCertificate,
		InstallCertificate,
	};

	static constexpr std::size_t kCertificateNameSize = 64U;
	static constexpr std::size_t kCertificateBase64Size = 3072U;

	struct ProvisioningRequest {
		ProvisioningOperation operation;
		std::size_t expectedSize;
		char certificateName[kCertificateNameSize];
		const char* base64Data;
	};

	static constexpr EventBits_t kNetworkConnected = 1U << 0U;
	static constexpr EventBits_t kMqttConnected = 1U << 1U;
	static constexpr uint32_t kWorkerWakeNotification = 1U << 0U;
	static constexpr uint32_t kWorkerPollIntervalMs = 250U;

	static void taskEntry(void* context);
	void run();
	bool submitProvisioningRequest(const ProvisioningRequest& request);
	bool processProvisioningRequest();
	bool attemptNetworkConnection();
	bool attemptMqttConnection();
	void synchronizeRtc();
	void publishStatus(bool networkConnected, bool mqttConnected);

	services::Sim7600Service& _sim7600Service;
	drivers::Sim7600PowerDriver& _sim7600PowerDriver;
	drivers::Sim7600Driver& _sim7600Driver;
	services::GpsService& _gpsService;
	services::SecurityService& _securityService;
	drivers::RtcDriver& _rtcDriver;
	middleware::CellularMqttClient& _mqttClient;
	core::Logger& _logger;
	EventGroupHandle_t _statusEvents;
	SemaphoreHandle_t _stoppedSemaphore;
	TaskHandle_t _taskHandle;
	std::atomic<bool> _stopRequested;
	std::atomic<bool> _networkEnabled;
	ConnectivityRequestMailbox _requests;
	QueueHandle_t _provisioningQueue;
	SemaphoreHandle_t _provisioningMutex;
	SemaphoreHandle_t _provisioningComplete;
	bool _provisioningResult;
};

}  // namespace app
}  // namespace fortico
