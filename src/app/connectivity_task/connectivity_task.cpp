#include "app/connectivity_task/connectivity_task.hpp"

#include <cstring>

#include "config/hardware_config.h"
#include "config/network_config.h"
#include "config/task_config.h"
#include "services/security/provisioning_service.h"
#include "utils/timer_utils.h"

namespace fortico {
namespace app {

ConnectivityTask::ConnectivityTask(
	drivers::Sim7600PowerDriver& sim7600PowerDriver,
	drivers::Sim7600Driver& sim7600Driver,
	services::Sim7600Service& sim7600Service,
	services::GpsService& gpsService,
	services::SecurityService& securityService,
	drivers::RtcDriver& rtcDriver,
	middleware::CellularMqttClient& mqttClient,
	core::Logger& logger)
	: _sim7600Service(sim7600Service),
	  _sim7600PowerDriver(sim7600PowerDriver),
	  _sim7600Driver(sim7600Driver), _gpsService(gpsService),
	  _securityService(securityService), _rtcDriver(rtcDriver),
	  _mqttClient(mqttClient), _logger(logger),
	  _statusEvents(xEventGroupCreate()),
	  _stoppedSemaphore(xSemaphoreCreateBinary()), _taskHandle(nullptr),
	  _stopRequested(false), _networkEnabled(false), _requests(),
	  _provisioningQueue(xQueueCreate(1U, sizeof(ProvisioningRequest))),
	  _provisioningMutex(xSemaphoreCreateMutex()),
	  _provisioningComplete(xSemaphoreCreateBinary()),
	  _provisioningResult(false)
{
}

bool ConnectivityTask::start(bool enableNetwork)
{
	if (_taskHandle != nullptr) {
		if (enableNetwork) {
			_networkEnabled.store(true, std::memory_order_release);
			(void)xTaskNotify(_taskHandle, kWorkerWakeNotification, eSetBits);
		}
		return true;
	}
	if (_statusEvents == nullptr || _stoppedSemaphore == nullptr ||
		_provisioningQueue == nullptr || _provisioningMutex == nullptr ||
		_provisioningComplete == nullptr) return false;
	_stopRequested.store(false, std::memory_order_release);
	_networkEnabled.store(enableNetwork, std::memory_order_release);
	(void)xSemaphoreTake(_stoppedSemaphore, 0U);
	return xTaskCreatePinnedToCore(
			   &ConnectivityTask::taskEntry, "ConnectivityTask",
			   tasks::NETWORK_SERVICE_TASK_STACK_WORDS, this,
			   tasks::NETWORK_SERVICE_TASK_PRIORITY, &_taskHandle,
			   tasks::CORE_NETWORK) == pdTRUE;
}

void ConnectivityTask::stop()
{
	if (_taskHandle == nullptr) return;
	_stopRequested.store(true, std::memory_order_release);
	(void)xTaskNotify(_taskHandle, kWorkerWakeNotification, eSetBits);
	(void)xSemaphoreTake(_stoppedSemaphore, portMAX_DELAY);
	_taskHandle = nullptr;
}

void ConnectivityTask::requestReconnect()
{
	_requests.requestReconnect();
	if (_statusEvents != nullptr) {
		(void)xEventGroupClearBits(_statusEvents,
								   kNetworkConnected | kMqttConnected);
	}
	if (_taskHandle != nullptr) {
		(void)xTaskNotify(_taskHandle, kWorkerWakeNotification, eSetBits);
	}
}

bool ConnectivityTask::networkConnected() const
{
	return _statusEvents != nullptr &&
		   (xEventGroupGetBits(_statusEvents) & kNetworkConnected) != 0U;
}

bool ConnectivityTask::mqttConnected() const
{
	return _statusEvents != nullptr &&
		   (xEventGroupGetBits(_statusEvents) & kMqttConnected) != 0U;
}

bool ConnectivityTask::hasModemCertificate(const char* certificateName)
{
	if (certificateName == nullptr ||
		std::strlen(certificateName) >= kCertificateNameSize) return false;
	ProvisioningRequest request{};
	request.operation = ProvisioningOperation::CheckCertificate;
	std::strncpy(request.certificateName, certificateName,
				 sizeof(request.certificateName) - 1U);
	return submitProvisioningRequest(request);
}

bool ConnectivityTask::installModemCertificate(
	const char* certificateName, std::size_t expectedSize,
	const char* base64Data)
{
	if (certificateName == nullptr || base64Data == nullptr ||
		std::strlen(certificateName) >= kCertificateNameSize) return false;
	const std::size_t encodedLength = std::strlen(base64Data);
	if (encodedLength == 0U || encodedLength > kCertificateBase64Size) {
		return false;
	}
	ProvisioningRequest request{};
	request.operation = ProvisioningOperation::InstallCertificate;
	request.expectedSize = expectedSize;
	std::strncpy(request.certificateName, certificateName,
				 sizeof(request.certificateName) - 1U);
	request.base64Data = base64Data;
	return submitProvisioningRequest(request);
}

bool ConnectivityTask::submitProvisioningRequest(
	const ProvisioningRequest& request)
{
	if (_taskHandle == nullptr || _provisioningQueue == nullptr ||
		_provisioningMutex == nullptr || _provisioningComplete == nullptr ||
		xSemaphoreTake(_provisioningMutex, portMAX_DELAY) != pdTRUE) {
		return false;
	}
	(void)xSemaphoreTake(_provisioningComplete, 0U);
	if (xQueueSend(_provisioningQueue, &request, 0U) != pdTRUE) {
		(void)xSemaphoreGive(_provisioningMutex);
		return false;
	}
	(void)xTaskNotify(_taskHandle, kWorkerWakeNotification, eSetBits);
	const bool completed =
		xSemaphoreTake(_provisioningComplete, portMAX_DELAY) == pdTRUE;
	const bool result = completed && _provisioningResult;
	(void)xSemaphoreGive(_provisioningMutex);
	return result;
}

void ConnectivityTask::taskEntry(void* context)
{
	static_cast<ConnectivityTask*>(context)->run();
}

void ConnectivityTask::run()
{
	if (_sim7600PowerDriver.init() == constants::ErrorCode::OK &&
		_sim7600PowerDriver.start()) {
		(void)_sim7600PowerDriver.powerOn();
	}

	const uint32_t retryIntervalMs = hardware::OFFLINE_RETRY_INTERVAL_MS;
	uint32_t lastModemAttemptMs = 0U;
	uint32_t lastNetworkAttemptMs = 0U;
	uint32_t lastNetworkCheckMs = 0U;
	uint32_t lastMqttAttemptMs = 0U;
	uint32_t lastTimeSyncMs = 0U;
	bool modemAttempted = false;
	bool diagnosticsAttempted = false;
	bool networkAttempted = false;
	bool networkCheckAttempted = false;
	bool mqttAttempted = false;
	bool timeSyncAttempted = false;
	bool gpsStarted = false;

	while (!_stopRequested.load(std::memory_order_acquire)) {
		const bool reconnectRequested = _requests.takeReconnectRequest();
		const uint32_t nowMs = static_cast<uint32_t>(utils::Timer::millis());
		if (reconnectRequested) {
			networkAttempted = false;
			mqttAttempted = false;
		}

		if (!_sim7600Service.isRunning() &&
			(!modemAttempted ||
			 nowMs - lastModemAttemptMs >= retryIntervalMs)) {
			if (gpsStarted) {
				_gpsService.stop();
				gpsStarted = false;
			}
			modemAttempted = true;
			lastModemAttemptMs = nowMs;
			if (!_sim7600Service.start()) {
				_logger.warn(logging::TAG_SIM7600,
							 "initialisation modem echouee; nouvelle tentative en arriere-plan");
			} else if (_networkEnabled.load(std::memory_order_acquire)) {
				services::Sim7600Diagnostics diagnostics{};
				const bool complete = _sim7600Service.runDiagnostics(diagnostics);
				diagnosticsAttempted = true;
				_logger.info(logging::TAG_SIM7600,
							 "diagnostic modem=%s SIM=%s signal=%s reseau=%s data=%s",
							 diagnostics.modemIdentified ? "ok" : "ko",
							 diagnostics.simReady ? "ok" : "ko",
							 diagnostics.signalAvailable ? "ok" : "ko",
							 diagnostics.networkRegistered ? "ok" : "ko",
							 diagnostics.dataConnected ? "ok" : "ko");
				if (!complete) {
					_logger.warn(logging::TAG_SIM7600,
								 "diagnostic modem partiel");
				}
			}
		}

		if (_sim7600Service.isRunning()) {
			(void)processProvisioningRequest();
			if (!_networkEnabled.load(std::memory_order_acquire)) {
				publishStatus(false, false);
				uint32_t notification = 0U;
				(void)xTaskNotifyWait(0U, kWorkerWakeNotification,
									  &notification,
									  pdMS_TO_TICKS(kWorkerPollIntervalMs));
				continue;
			}
			if (!diagnosticsAttempted) {
				services::Sim7600Diagnostics diagnostics{};
				const bool complete = _sim7600Service.runDiagnostics(diagnostics);
				diagnosticsAttempted = true;
				_logger.info(logging::TAG_SIM7600,
							 "diagnostic modem=%s SIM=%s signal=%s reseau=%s data=%s",
							 diagnostics.modemIdentified ? "ok" : "ko",
							 diagnostics.simReady ? "ok" : "ko",
							 diagnostics.signalAvailable ? "ok" : "ko",
							 diagnostics.networkRegistered ? "ok" : "ko",
							 diagnostics.dataConnected ? "ok" : "ko");
				if (!complete) {
					_logger.warn(logging::TAG_SIM7600,
								 "diagnostic modem partiel");
				}
			}
			if (!gpsStarted) {
				gpsStarted = _gpsService.start();
				if (!gpsStarted) {
					_logger.warn(logging::TAG_GPS,
								 "demarrage GNSS echoue; nouvelle tentative en arriere-plan");
				}
			}

			bool dataConnected = _sim7600Service.isDataConnected();
			if (reconnectRequested) {
				_mqttClient.disconnect();
				dataConnected = false;
			}
			if (dataConnected && !reconnectRequested &&
				(!networkCheckAttempted ||
				 nowMs - lastNetworkCheckMs >= retryIntervalMs)) {
				networkCheckAttempted = true;
				lastNetworkCheckMs = nowMs;
				if (!_sim7600Service.waitForNetwork(1000U)) {
					dataConnected = false;
					networkAttempted = false;
				}
			}
			if (!dataConnected &&
				(!networkAttempted ||
				 nowMs - lastNetworkAttemptMs >= retryIntervalMs)) {
				networkAttempted = true;
				lastNetworkAttemptMs = nowMs;
				dataConnected = attemptNetworkConnection();
			}

			if (dataConnected) {
				publishStatus(true, mqttConnected());
				if (!_mqttClient.connected() &&
					(!mqttAttempted ||
					 nowMs - lastMqttAttemptMs >= retryIntervalMs)) {
					mqttAttempted = true;
					lastMqttAttemptMs = nowMs;
					const bool connected = attemptMqttConnection();
					publishStatus(true, connected);
				} else if (_mqttClient.connected()) {
					publishStatus(true, true);
				} else {
					publishStatus(true, false);
				}

				if (!timeSyncAttempted ||
					nowMs - lastTimeSyncMs >=
						hardware::NETWORK_TIME_SYNC_INTERVAL_MS) {
					timeSyncAttempted = true;
					lastTimeSyncMs = nowMs;
					synchronizeRtc();
				}
			} else {
				publishStatus(false, false);
			}
			if (gpsStarted) _gpsService.update();
		} else {
			publishStatus(false, false);
		}

		uint32_t notification = 0U;
		(void)xTaskNotifyWait(0U, kWorkerWakeNotification, &notification,
							  pdMS_TO_TICKS(kWorkerPollIntervalMs));
	}

	_mqttClient.disconnect();
	if (gpsStarted) _gpsService.stop();
	_sim7600Service.stop();
	_sim7600PowerDriver.stop();
	publishStatus(false, false);
	(void)xSemaphoreGive(_stoppedSemaphore);
	_taskHandle = nullptr;
	vTaskDelete(nullptr);
}

bool ConnectivityTask::processProvisioningRequest()
{
	if (_provisioningQueue == nullptr) return false;
	ProvisioningRequest request{};
	if (xQueueReceive(_provisioningQueue, &request, 0U) != pdTRUE) {
		return false;
	}

	bool result = false;
	if (request.operation == ProvisioningOperation::CheckCertificate) {
		result = _sim7600Driver.hasCertificate(request.certificateName);
	} else if (request.operation == ProvisioningOperation::InstallCertificate &&
			   request.expectedSize ==
				   services::ProvisioningService::ExpectedHiveMqCertSize) {
		uint8_t certificate[2048]{};
		std::size_t certificateLength = sizeof(certificate);
		if (request.base64Data != nullptr) {
			const std::size_t encodedLength = std::strlen(request.base64Data);
			if (services::ProvisioningService::base64Decode(
					request.base64Data, encodedLength, certificate,
					certificateLength) &&
				certificateLength == request.expectedSize) {
				(void)_sim7600Driver.deleteCertificate(request.certificateName);
				result = _sim7600Driver.downloadCertificate(
					request.certificateName, certificate, certificateLength);
			}
		}
	}
	_provisioningResult = result;
	if (_provisioningComplete != nullptr) {
		(void)xSemaphoreGive(_provisioningComplete);
	}
	return true;
}

bool ConnectivityTask::attemptNetworkConnection()
{
	_logger.info(logging::TAG_SYSTEM_CONTROLLER,
				 "tentative de connexion reseau en arriere-plan");
	if (!_sim7600Service.waitForNetwork(
			network::MODEM_NETWORK_REGISTRATION_TIMEOUT_MS)) {
		_logger.warn(logging::TAG_SYSTEM_CONTROLLER,
					 "enregistrement reseau indisponible; controle relais non bloque");
		return false;
	}
	if (!_sim7600Service.connectInternet(
			network::APN_DEFAULT, network::MODEM_PDP_CONTEXT_TIMEOUT_MS)) {
		_logger.warn(logging::TAG_SYSTEM_CONTROLLER,
					 "activation PDP echouee; nouvelle tentative en arriere-plan");
		return false;
	}

	_logger.info(logging::TAG_SYSTEM_CONTROLLER,
				 "connexion reseau cellulaire etablie en arriere-plan");
	return true;
}

bool ConnectivityTask::attemptMqttConnection()
{
	types::MqttCredentials credentials{};
	if (!_securityService.loadMqttCredentials(credentials) ||
		!_mqttClient.setCredentials(credentials)) {
		_logger.warn(logging::TAG_MQTT,
					 "identifiants MQTT indisponibles; nouvelle tentative en arriere-plan");
		return false;
	}

	if (!_mqttClient.connect(network::MQTT_BROKER_HOST,
							 network::MQTT_BROKER_PORT)) {
		_logger.warn(logging::TAG_MQTT,
					 "connexion MQTT/TLS echouee; nouvelle tentative en arriere-plan");
		return false;
	}

	_logger.info(logging::TAG_MQTT,
				 "connexion MQTT/TLS etablie en arriere-plan");
	return true;
}

void ConnectivityTask::synchronizeRtc()
{
	if (!_sim7600Service.isDataConnected()) return;
	drivers::RtcDateTime networkTime{};
	if (!_sim7600Service.getNetworkDateTime(networkTime)) {
		_logger.warn(logging::TAG_SYSTEM_CONTROLLER,
					 "synchronisation RTC depuis le reseau impossible");
		return;
	}
	if (_rtcDriver.setDateTime(networkTime)) {
		_logger.info(logging::TAG_SYSTEM_CONTROLLER,
					 "RTC synchronisee depuis l'heure reseau");
	} else {
		_logger.warn(logging::TAG_SYSTEM_CONTROLLER,
					 "ecriture de l'heure reseau dans le RTC impossible");
	}
}

void ConnectivityTask::publishStatus(bool networkIsConnected,
									bool mqttIsConnected)
{
	if (_statusEvents == nullptr) return;
	EventBits_t bitsToSet = 0U;
	if (networkIsConnected) bitsToSet |= kNetworkConnected;
	if (mqttIsConnected) bitsToSet |= kMqttConnected;
	(void)xEventGroupClearBits(_statusEvents,
							   kNetworkConnected | kMqttConnected);
	if (bitsToSet != 0U) (void)xEventGroupSetBits(_statusEvents, bitsToSet);
}

}  // namespace app
}  // namespace fortico
