#include "app/system_controller/system_controller.hpp"

#include "config/hardware_config.h"
#include "config/network_config.h"
#include "utils/timer_utils.h"

namespace fortico {
namespace app {

SystemController::SystemController(
	core::EventBus& eventBus, managers::AlertManager& alertManager,
	services::AuthenticationService& authenticationService,
	services::RfidService& rfidService,
	drivers::Sim7600PowerDriver& sim7600PowerDriver,
	services::Sim7600Service& sim7600Service,
	services::GpsService& gpsService,
	services::DiagnosticsService& diagnosticsService,
	core::PowerMonitor& powerMonitor, core::ErrorHandler& errorHandler,
	core::Logger& logger,
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
	  _sim7600PowerDriver(sim7600PowerDriver), _sim7600Service(sim7600Service),
	  _gpsService(gpsService),
	  _diagnosticsService(diagnosticsService),
	  _powerMonitor(powerMonitor), _errorHandler(errorHandler),
	  _logger(logger),
	  _securityService(securityService), _storageService(storageService),
	  _telemetryService(telemetryService), _powerManager(powerManager),
	  _otaManager(otaManager), _relayDriver(relayDriver),
	  _relayManager(relayManager), _fuelDriver(fuelDriver),
	  _fuelManager(fuelManager), _rtcDriver(rtcDriver), _sdDriver(sdDriver),
	  _mqttClient(mqttClient),
	  _initialized(false), _running(false), _bootStage(BootStage::Idle),
	  _lastNetworkTimeSyncMs(0U), _lastRfidHealthy(false),
	  _lastSdHealthy(false), _healthStateKnown(false), _lastHeartbeatMs(0U),
	  _lastAuthenticationTimestamp(0U), _lastGpsLogMs(0U),
	  _lastAlertTimestamp(0U), _lastAlertCode(types::AlertCode::DIAGNOSTIC_WARNING),
	  _hasProcessedAlert(false), _lastBatteryLogMs(0U)
{
}

bool SystemController::init()
{
	if (_initialized) return true;
	setBootStage(BootStage::Starting, "initialisation des composants Fortico");
	if (!_eventBus.init()) {
		return failBoot("initialisation EventBus impossible",
						constants::ErrorCode::UNKNOWN);
	}
	if (!_powerMonitor.init()) {
		return failBoot("initialisation mesure batterie impossible",
						constants::ErrorCode::BATTERY_SENSOR_FAILURE);
	}
	_powerManager.init();
	_otaManager.init();
	_relayManager.init();
	if (_fuelDriver.init() != constants::ErrorCode::OK ||
		_rtcDriver.init() != constants::ErrorCode::OK) {
		return failBoot("initialisation driver fuel ou RTC impossible",
						constants::ErrorCode::UNKNOWN);
	}
	const constants::ErrorCode sdInitResult = _sdDriver.init();
	if (sdInitResult != constants::ErrorCode::OK) {
		_logger.warn(logging::TAG_SYSTEM_CONTROLLER,
					 "initialisation SD echouee: code=%d",
					 static_cast<int>(sdInitResult));
	}
	if (!_securityService.start()) {
		return failBoot("initialisation stockage securise impossible",
						constants::ErrorCode::NVS_READ_FAILED);
	}
	if (!_storageService.start()) {
		_securityService.stop();
		return failBoot("initialisation service de stockage impossible",
						constants::ErrorCode::NVS_READ_FAILED);
	}
	_alertManager.init();
	_alertManager.subscribeTo(_eventBus);
	_initialized = true;
	return true;
}

bool SystemController::start()
{
	if (!_initialized || _running) return _running;
	setBootStage(BootStage::Starting, "demarrage des services Fortico");
	if (!_authenticationService.start()) {
		return failBoot("demarrage authentification impossible",
						constants::ErrorCode::UNKNOWN);
	}
	if (!_telemetryService.start()) {
		_authenticationService.stop();
		return failBoot("demarrage telemetrie impossible",
						constants::ErrorCode::UNKNOWN);
	}
	if (!_diagnosticsService.start()) {
		_authenticationService.stop();
		_telemetryService.stop();
		return failBoot("demarrage diagnostics impossible",
						constants::ErrorCode::UNKNOWN);
	}
	setBootStage(BootStage::PoweringModem, "impulsion K/PWRKEY SIM7600");
	if (_sim7600PowerDriver.init() != constants::ErrorCode::OK ||
		!_sim7600PowerDriver.start() || !_sim7600PowerDriver.powerOn()) {
		failBoot("echec du controle K/PWRKEY SIM7600",
				 constants::ErrorCode::MODEM_AT_TIMEOUT);
		_authenticationService.stop();
		_diagnosticsService.stop();
		_telemetryService.stop();
		return false;
	}
	setBootStage(BootStage::InitializingModem, "handshake AT SIM7600");
	if (!_sim7600Service.start()) {
		failBoot("SIM7600 ne repond pas aux commandes AT",
				 constants::ErrorCode::MODEM_AT_TIMEOUT);
		_sim7600PowerDriver.stop();
		_authenticationService.stop();
		_diagnosticsService.stop();
		_telemetryService.stop();
		return false;
	}
	setBootStage(BootStage::InitializingGnss, "activation du GNSS SIM7600");
	if (!_gpsService.start()) {
		failBoot("activation GNSS impossible", constants::ErrorCode::GNSS_TIMEOUT);
		_sim7600Service.stop();
		_sim7600PowerDriver.stop();
		_authenticationService.stop();
		_diagnosticsService.stop();
		_telemetryService.stop();
		return false;
	}
	setBootStage(BootStage::RegisteringNetwork, "enregistrement reseau mobile");
	if (!_sim7600Service.waitForNetwork(
			network::MODEM_NETWORK_REGISTRATION_TIMEOUT_MS) ||
		!_sim7600Service.connectInternet(
			network::APN_DEFAULT, network::MODEM_PDP_CONTEXT_TIMEOUT_MS)) {
		failBoot("enregistrement reseau ou contexte PDP impossible",
				 constants::ErrorCode::MODEM_REGISTRATION_FAILED);
		_gpsService.stop();
		_sim7600Service.stop();
		_sim7600PowerDriver.stop();
		_authenticationService.stop();
		_diagnosticsService.stop();
		_telemetryService.stop();
		return false;
	}
	// Première mise à l'heure dès que le contexte de données est disponible.
	// Les vérifications suivantes sont faites périodiquement dans update().
	synchronizeRtcFromNetwork();
	_lastNetworkTimeSyncMs = utils::Timer::millis();
	logSim7600Diagnostics();
	setBootStage(BootStage::ConnectingMqtt, "connexion MQTT cellulaire");
	types::MqttCredentials credentials{};
	if (!_securityService.loadMqttCredentials(credentials) ||
		!_mqttClient.setCredentials(credentials) ||
		!_mqttClient.connect(network::MQTT_BROKER_HOST, network::MQTT_BROKER_PORT)) {
		failBoot("connexion MQTT impossible", constants::ErrorCode::MQTT_CONNECT_FAILED);
		_gpsService.stop();
		_sim7600Service.stop();
		_sim7600PowerDriver.stop();
		_authenticationService.stop();
		_diagnosticsService.stop();
		_telemetryService.stop();
		return false;
	}
	setBootStage(BootStage::StartingServices, "demarrage RFID et EventBus");
	if (!_rfidService.start()) {
		_mqttClient.disconnect();
		_gpsService.stop();
		_sim7600Service.stop();
		_sim7600PowerDriver.stop();
		_authenticationService.stop();
		_diagnosticsService.stop();
		_telemetryService.stop();
		return failBoot("demarrage RFID impossible", constants::ErrorCode::UNKNOWN);
	}
	_logger.info(logging::TAG_SYSTEM_CONTROLLER,
				 "RfidTask demarrage demande");
	if (!_eventBus.start()) {
		_rfidService.stop();
		_mqttClient.disconnect();
		_gpsService.stop();
		_sim7600Service.stop();
		_sim7600PowerDriver.stop();
		_authenticationService.stop();
		_diagnosticsService.stop();
		_telemetryService.stop();
		return failBoot("demarrage EventBus impossible", constants::ErrorCode::UNKNOWN);
	}
	if (!_relayDriver.start() || !_fuelDriver.start() || !_rtcDriver.start()) {
		stop();
		return failBoot("demarrage driver relais, fuel ou RTC impossible",
						constants::ErrorCode::UNKNOWN);
	}
	const bool sdStarted = _sdDriver.start();
	if (!sdStarted || !_sdDriver.isHealthy()) {
		_logger.warn(logging::TAG_SYSTEM_CONTROLLER,
					 "SD indisponible: le diagnostic peut declencher une alerte");
	} else {
		_logger.info(logging::TAG_STORAGE,
					 "journal SD actif: /fortico.log");
	}

	// Étape initiale : le relais maintient le moteur coupé par défaut.
	_relayManager.submit({types::RelayCmd::CUT, types::RelaySource::AUTH, utils::Timer::millis()});
	_telemetryService.setActiveTracking(false);
	_logger.info(logging::TAG_SYSTEM_CONTROLLER,
				 "ETAPE INITIALE : Moteur verrouille (relais coupe). En attente du badge chauffeur...");

	_running = true;
	_lastNetworkTimeSyncMs = utils::Timer::millis();
	setBootStage(BootStage::Ready, "Fortico operationnel");
	return true;
}

void SystemController::update()
{
	if (!_running) return;

	_gpsService.update();
	logGpsPosition();

	const uint64_t rfidStartMs = utils::Timer::millis();
	_rfidService.update();

	// Synchronisation de l'état du relais et du carburant dans le service de télémétrie
	_telemetryService.setRelay(_relayManager.state());
	_telemetryService.setFuel(_fuelManager.getLevel());

	// Traitement de l'authentification chauffeur par RFID
	const types::AuthenticationDecision decision = _authenticationService.decision();
	if (decision.timestamp != 0U &&
		decision.timestamp != _lastAuthenticationTimestamp) {
		_lastAuthenticationTimestamp = decision.timestamp;
		const bool isAuthorized = (decision.result == types::AuthenticationResult::AUTHORIZED);

		if (isAuthorized) {
			// Badge vérifié avec succès : moteur autorisé par le relais et passage à l'étape de suivi actif
			_relayManager.submit({types::RelayCmd::ENGAGE, types::RelaySource::AUTH, decision.timestamp});
			_telemetryService.setAuthentication(decision.result);
			_telemetryService.setActiveTracking(true);
			_logger.info(logging::TAG_SYSTEM_CONTROLLER,
						 "CHAUFFEUR VERIFIE (UID: %s). Moteur autorise par le relais. Passage a l'etape suivante : suivi GPS et MQTT actives.",
						 decision.uid);
			// Notification MQTT immédiate pour le déblocage moteur
			if (_mqttClient.connected()) {
				_telemetryService.forcePublish();
			}
		} else {
			// Badge non reconnu ou non autorisé : moteur maintenu coupé, alerte déclenchée
			_relayManager.submit({types::RelayCmd::CUT, types::RelaySource::AUTH, decision.timestamp});
			_telemetryService.setAuthentication(decision.result);
			_telemetryService.setActiveTracking(false);
			_alertManager.raise(types::AlertCode::AUTH_DENIED, types::AlertSeverity::MEDIUM, decision.timestamp);
			_logger.warn(logging::TAG_SYSTEM_CONTROLLER,
						 "CHAUFFEUR NON RECONNU (UID: %s). Moteur maintenu coupe par le relais. Alerte emise.",
						 decision.uid);
		}
	}

	const uint64_t rfidElapsedMs = utils::Timer::millis() - rfidStartMs;
	const bool rfidHealthy = _rfidService.isRunning();
	const bool sdHealthy = _sdDriver.isHealthy();
	if (rfidElapsedMs >= 200U) {
		_logger.warn(logging::TAG_SYSTEM_CONTROLLER,
					 "cycle RFID lent: duree=%llu ms, sante=%s",
					 static_cast<unsigned long long>(rfidElapsedMs),
					 rfidHealthy ? "oui" : "non");
	}
	if (!_healthStateKnown || rfidHealthy != _lastRfidHealthy) {
		_logger.info(logging::TAG_SYSTEM_CONTROLLER,
					 "etat RFID: %s, cycle=%llu ms",
					 rfidHealthy ? "sain" : "indisponible",
					 static_cast<unsigned long long>(rfidElapsedMs));
	}
	if (!_healthStateKnown || sdHealthy != _lastSdHealthy) {
		_logger.info(logging::TAG_SYSTEM_CONTROLLER,
					 "etat SD: %s", sdHealthy ? "sain" : "indisponible");
	}
	_lastRfidHealthy = rfidHealthy;
	_lastSdHealthy = sdHealthy;
	_healthStateKnown = true;

	if (utils::Timer::millis() - _lastHeartbeatMs >= 10000U) {
		_logger.info(logging::TAG_SYSTEM_CONTROLLER,
					 "heartbeat: controleur vivant, RFID=%s, SD=%s, suivi_actif=%s, cycles=%lu, uid=%lu, echecs=%lu",
					 rfidHealthy ? "actif" : "indisponible",
					 sdHealthy ? "sain" : "indisponible",
					 _telemetryService.isActiveTracking() ? "oui" : "attente_badge",
					 static_cast<unsigned long>(_rfidService.updateCount()),
					 static_cast<unsigned long>(_rfidService.uidCount()),
					 static_cast<unsigned long>(_rfidService.readFailureCount()));
		_lastHeartbeatMs = utils::Timer::millis();
	}

	const uint64_t nowMs = utils::Timer::millis();
	if (nowMs - _lastNetworkTimeSyncMs >=
		hardware::NETWORK_TIME_SYNC_INTERVAL_MS) {
		synchronizeRtcFromNetwork();
		_lastNetworkTimeSyncMs = nowMs;
	}

	_alertManager.update();
	_alertManager.process();
	processAlerts();

	_powerMonitor.update();
	_powerManager.update();
	logBatteryStatus();

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

void SystemController::processAlerts()
{
	if (!_alertManager.hasPendingAlert()) return;
	const types::AlertEvent alert = _alertManager.pendingAlert();
	if (!_hasProcessedAlert || alert.timestamp != _lastAlertTimestamp || alert.code != _lastAlertCode) {
		_hasProcessedAlert = true;
		_lastAlertTimestamp = alert.timestamp;
		_lastAlertCode = alert.code;
		const char* alertName = "INCONNU";
		switch (alert.code) {
			case types::AlertCode::AUTH_DENIED: alertName = "BADGE_NON_AUTORISE"; break;
			case types::AlertCode::SOS_BUTTON: alertName = "BOUTON_SOS_PRESSE"; break;
			case types::AlertCode::BATTERY_LOW: alertName = "BATTERIE_CRITIQUE"; break;
			case types::AlertCode::FUEL_ANOMALY: alertName = "ANOMALIE_CARBURANT"; break;
			case types::AlertCode::GPS_LOST: alertName = "SIGNAL_GPS_PERDU"; break;
			case types::AlertCode::RELAY_FAULT: alertName = "DEFAILLANCE_RELAIS"; break;
			case types::AlertCode::NETWORK_LOST: alertName = "RESEAU_CELLULAIRE_PERDU"; break;
			case types::AlertCode::DIAGNOSTIC_WARNING: alertName = "AVERTISSEMENT_DIAGNOSTIC"; break;
		}
		_logger.warn(logging::TAG_ALERT,
					 "ALERTE DETECTEE: %s (severite=%u)",
					 alertName, static_cast<unsigned int>(alert.severity));
		if (_mqttClient.connected()) {
			_telemetryService.forcePublish();
		}
	}
}

void SystemController::logBatteryStatus()
{
	constexpr uint64_t batteryLogIntervalMs = 60000U; // toutes les 60s
	const uint64_t nowMs = utils::Timer::millis();
	if (nowMs - _lastBatteryLogMs < batteryLogIntervalMs) return;
	_lastBatteryLogMs = nowMs;

	const core::PowerState pState = _powerMonitor.state();
	const char* stateStr = "NORMALE";
	if (pState == core::PowerState::LOW_POWER) stateStr = "FAIBLE";
	else if (pState == core::PowerState::CRITICAL) stateStr = "CRITIQUE";

	_logger.info(logging::TAG_POWER,
				 "Batterie: etat=%s (alimentation stable)",
				 stateStr);
}

void SystemController::stop()
{
	_rfidService.stop();
	_mqttClient.disconnect();
	_gpsService.stop();
	_sim7600Service.stop();
	_sim7600PowerDriver.stop();
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
BootStage SystemController::bootStage() const { return _bootStage; }

void SystemController::setBootStage(BootStage stage, const char* message)
{
	_bootStage = stage;
	_logger.info(logging::TAG_SYSTEM_CONTROLLER, "%s", message);
}

bool SystemController::failBoot(const char* message, constants::ErrorCode error)
{
	_bootStage = BootStage::Failed;
	_errorHandler.report(error);
	_logger.error(logging::TAG_SYSTEM_CONTROLLER, "%s", message);
	return false;
}

void SystemController::logSim7600Diagnostics()
{
	services::Sim7600Diagnostics diagnostics{};
	const bool complete = _sim7600Service.runDiagnostics(diagnostics);
	_logger.info(logging::TAG_SIM7600,
				 "diagnostic: modem=%s sim=%s signal=%s reseau=%s operateur=%s radio=%s paquet=%s data=%s",
				 diagnostics.modemIdentified ? "ok" : "ko",
				 diagnostics.simReady ? "ok" : "ko",
				 diagnostics.signalAvailable ? "ok" : "ko",
				 diagnostics.networkRegistered ? "ok" : "ko",
				 diagnostics.operatorAvailable ? "ok" : "ko",
				 diagnostics.radioServiceAvailable ? "ok" : "ko",
				 diagnostics.packetAttached ? "ok" : "ko",
				 diagnostics.dataConnected ? "ok" : "ko");
	if (!complete) {
		_logger.warn(logging::TAG_SIM7600,
					 "diagnostic partiel: verifier SIM, antenne et enregistrement reseau");
	}
}

void SystemController::logGpsPosition()
{
	constexpr uint64_t gpsLogIntervalMs = 5000U;
	constexpr uint64_t gpsStaleAfterMs = 3000U;
	const uint64_t nowMs = utils::Timer::millis();
	if (nowMs - _lastGpsLogMs < gpsLogIntervalMs) return;
	_lastGpsLogMs = nowMs;

	const types::GpsData& gps = _gpsService.data();
	if (!gps.fixValid) {
		_logger.info(logging::TAG_TRACKING,
					 "GPS: attente fix, mode=%u satellites=%u HDOP=%.2f PDOP=%.2f",
					 static_cast<unsigned int>(gps.fixMode),
					 static_cast<unsigned int>(gps.satellites), gps.hdop, gps.pdop);
		return;
	}

	const uint64_t ageMs = nowMs - gps.timestamp;
	if (ageMs > gpsStaleAfterMs) {
		_logger.info(logging::TAG_TRACKING,
					 "GPS: dernier fix connu, age=%llums lat=%.6f lon=%.6f alt=%.0fm vitesse=%.1fkm/h cap=%.0f sat=%u HDOP=%.1f PDOP=%.1f",
					 static_cast<unsigned long long>(ageMs), gps.latitude,
					 gps.longitude, gps.altitudeMeters, gps.speedKmh,
					 gps.headingDeg, static_cast<unsigned int>(gps.satellites),
					 gps.hdop, gps.pdop);
		return;
	}

	_logger.info(logging::TAG_TRACKING,
				 "GPS: fix=%u lat=%.6f lon=%.6f alt=%.0fm vitesse=%.1fkm/h cap=%.0f sat=%u HDOP=%.1f PDOP=%.1f",
				 static_cast<unsigned int>(gps.fixMode), gps.latitude,
				 gps.longitude, gps.altitudeMeters, gps.speedKmh, gps.headingDeg,
				 static_cast<unsigned int>(gps.satellites), gps.hdop, gps.pdop);
}

void SystemController::synchronizeRtcFromNetwork()
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

}  // namespace app
}  // namespace fortico
