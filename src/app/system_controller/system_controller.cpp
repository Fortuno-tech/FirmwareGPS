#include "app/system_controller/system_controller.hpp"
#include "app/system_controller/remote_relay_policy.hpp"

#include <Arduino.h>

#include "config/hardware_config.h"
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
	middleware::CellularMqttClient& mqttClient,
	ConnectivityTask& connectivityTask)
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
	  _mqttClient(mqttClient), _connectivityTask(connectivityTask),
	  _remoteRelayCommandMailbox(RemoteRelayCommandMailbox::instance()),
	  _systemEventMailbox(SystemEventMailbox::instance()),
	  _initialized(false), _running(false), _bootStage(BootStage::Idle),
	  _lastRfidHealthy(false),
	  _lastSdHealthy(false), _healthStateKnown(false), _lastHeartbeatMs(0U),
	  _lastAuthenticationTimestamp(0U), _lastGpsLogMs(0U),
	  _lastAlertTimestamp(0U), _lastAlertCode(types::AlertCode::DIAGNOSTIC_WARNING),
	  _hasProcessedAlert(false), _lastBatteryLogMs(0U), _lastStatusPublishMs(0U),
	  _lastPublishedRelayState(types::RelayState::LOCKED), _relayStateKnown(false),
	  _systemState(types::SystemState::BOOT), _lastGpsMqttPublishMs(0U),
	  _killLocked(false), _lastMqttConnected(false)
{
}

bool SystemController::init()
{
	if (_initialized) return true;
	if (!_remoteRelayCommandMailbox.ready() || !_systemEventMailbox.ready()) {
		return failBoot("initialisation mailbox de controle impossible",
						constants::ErrorCode::UNKNOWN);
	}
	setBootStage(BootStage::InitHardware, "INIT_HARDWARE: initialisation des composants");
	_systemState = types::SystemState::INIT_HARDWARE;
	if (!_eventBus.init()) {
		_alertManager.setPattern(managers::AlertPattern::ERROR);
		transitionTo(types::SystemState::ERROR);
		return failBoot("initialisation EventBus impossible",
						constants::ErrorCode::UNKNOWN);
	}
	if (!_powerMonitor.init()) {
		_alertManager.setPattern(managers::AlertPattern::ERROR);
		transitionTo(types::SystemState::ERROR);
		return failBoot("initialisation mesure batterie impossible",
						constants::ErrorCode::BATTERY_SENSOR_FAILURE);
	}
	_powerManager.init();
	_otaManager.init();
	_relayManager.init();
	if (_fuelDriver.init() != constants::ErrorCode::OK ||
		_rtcDriver.init() != constants::ErrorCode::OK) {
		_alertManager.setPattern(managers::AlertPattern::ERROR);
		transitionTo(types::SystemState::ERROR);
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
		_alertManager.setPattern(managers::AlertPattern::ERROR);
		transitionTo(types::SystemState::ERROR);
		return failBoot("initialisation stockage securise impossible",
						constants::ErrorCode::NVS_READ_FAILED);
	}
	if (!_storageService.start()) {
		_securityService.stop();
		_alertManager.setPattern(managers::AlertPattern::ERROR);
		transitionTo(types::SystemState::ERROR);
		return failBoot("initialisation service de stockage impossible",
						constants::ErrorCode::NVS_READ_FAILED);
	}
	_alertManager.init();
	_alertManager.subscribeTo(_eventBus);

	// Souscriptions EventBus pour le flux de contrôle
	_eventBus.subscribe(constants::EventID::SOS_BUTTON_PRESSED, this);
	_eventBus.subscribe(constants::EventID::REMOTE_STOP_COMMAND, this);
	_eventBus.subscribe(constants::EventID::REMOTE_KILL_COMMAND, this);
	_eventBus.subscribe(constants::EventID::SOS_ACK_RECEIVED, this);
	_eventBus.subscribe(constants::EventID::NETWORK_LOST, this);
	_eventBus.subscribe(constants::EventID::NETWORK_CONNECTED, this);
	_eventBus.subscribe(constants::EventID::BATTERY_LOW, this);
	_eventBus.subscribe(constants::EventID::FUEL_ANOMALY, this);
	_eventBus.subscribe(constants::EventID::BADGE_REMOVED, this);

	_initialized = true;
	return true;
}

bool SystemController::start()
{
	if (!_initialized || _running) return _running;
	setBootStage(BootStage::StartingServices, "demarrage des services Fortico");
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
	setBootStage(BootStage::StartingServices,
				 "initialisation des actionneurs avant la connectivite");
	if (!_relayDriver.start() || !_fuelDriver.start() || !_rtcDriver.start()) {
		stop();
		return failBoot("demarrage driver relais, fuel ou RTC impossible",
						constants::ErrorCode::UNKNOWN);
	}
	_relayManager.submit({types::RelayCmd::CUT, types::RelaySource::AUTH,
						  utils::Timer::millis()});
	_relayManager.process();

	setBootStage(BootStage::StartingServices, "demarrage RFID et EventBus");
	if (!_rfidService.start()) {
		_logger.warn(logging::TAG_SYSTEM_CONTROLLER, "demarrage RFID en cours");
	}
	_eventBus.start();

	setBootStage(BootStage::StartingServices,
				 "demarrage connectivite en tache dediee");
	if (!_connectivityTask.start()) {
		stop();
		return failBoot("demarrage de ConnectivityTask impossible",
						constants::ErrorCode::UNKNOWN);
	}
	const bool sdStarted = _sdDriver.start();
	if (!sdStarted || !_sdDriver.isHealthy()) {
		_logger.warn(logging::TAG_SYSTEM_CONTROLLER,
					 "SD indisponible: le diagnostic peut declencher une alerte");
	} else {
		_logger.info(logging::TAG_STORAGE,
					 "journal SD actif: /fortico/log/fortico.log");
	}

	// Étape initiale : le relais maintient le moteur coupé par défaut
	_relayManager.submit({types::RelayCmd::CUT, types::RelaySource::AUTH, utils::Timer::millis()});
	_telemetryService.setActiveTracking(false);
	transitionTo(types::SystemState::WAIT_BADGE);
	_alertManager.setPattern(managers::AlertPattern::WAIT_BADGE);
	_logger.info(logging::TAG_SYSTEM_CONTROLLER,
				 "ETAPE WAIT_BADGE : Moteur verrouille (relais coupe). En attente du badge chauffeur (LED jaune clignotante)...");

	_running = true;
	setBootStage(BootStage::Ready, "Fortico operationnel - WAIT_BADGE");
	return true;
}

void SystemController::update()
{
	if (!_running) return;
	const uint64_t updateStartMs = utils::Timer::millis();
	if (logging::ENABLE_HOT_PATH_PERF_LOGS) {
		Serial.printf("[PERF][SYSTEM] update START t=%llu\n",
					  static_cast<unsigned long long>(updateStartMs));
	}
	processSystemEvents();
	processRemoteRelayCommands();
	if (logging::ENABLE_HOT_PATH_PERF_LOGS) {
		Serial.printf("[PERF][SYSTEM] update END duration=%llu\n",
					  static_cast<unsigned long long>(utils::Timer::millis() - updateStartMs));
	}

	logGpsPosition();
	const uint64_t rfidStartMs = utils::Timer::millis();
	_rfidService.update();
	services::RfidService::Result rfidResult{};
	const bool hasRfidResult = _rfidService.receiveResult(rfidResult);

	// Synchronisation de l'état du relais et du carburant dans le service de télémétrie
	_telemetryService.setRelay(_relayManager.state());
	_telemetryService.setFuel(_fuelManager.getLevel());

	// Traitement selon la Machine à États Fortico (FSM)
	const types::AuthenticationDecision decision = _authenticationService.decision();
	const bool hasNewDecision = (hasRfidResult && decision.timestamp != 0U &&
								 decision.timestamp != _lastAuthenticationTimestamp);

	if (_systemState == types::SystemState::WAIT_BADGE) {
		_relayManager.submit({types::RelayCmd::CUT, types::RelaySource::AUTH, utils::Timer::millis()});
		_telemetryService.setActiveTracking(false);
		if (_alertManager.pattern() != managers::AlertPattern::WAIT_BADGE) {
			_alertManager.setPattern(managers::AlertPattern::WAIT_BADGE);
		}

		if (hasNewDecision) {
			_lastAuthenticationTimestamp = decision.timestamp;
			const bool isAuthorized = (decision.result == types::AuthenticationResult::AUTHORIZED);
			drivers::RtcDateTime rfidEventTime{};
			const bool rfidTimeValid = _rtcDriver.readDateTime(rfidEventTime);
			_telemetryService.publishRfid(
				decision.uid, isAuthorized, rfidTimeValid ? &rfidEventTime : nullptr);

			if (isAuthorized &&
				!localAuthenticationMayStartEngine(_systemState, _killLocked)) {
				transitionTo(types::SystemState::ENGINE_KILL);
				_relayManager.submit({types::RelayCmd::EMERGENCY_CUT,
									  types::RelaySource::REMOTE_COMMAND,
									  decision.timestamp});
				_telemetryService.setActiveTracking(false);
				_logger.warn(logging::TAG_SYSTEM_CONTROLLER,
							 "badge autorise refuse: verrou KILL actif jusqu'au redemarrage");
			} else if (isAuthorized) {
				transitionTo(types::SystemState::TRACKING_ACTIVE);
				_relayManager.submit({types::RelayCmd::ENGAGE, types::RelaySource::AUTH, decision.timestamp});
				_telemetryService.setAuthentication(decision.result);
				_telemetryService.setActiveTracking(true);
				_alertManager.setPattern(managers::AlertPattern::TRACKING_OK);
				_logger.info(logging::TAG_SYSTEM_CONTROLLER,
							 "CHAUFFEUR VERIFIE (UID: %s). Moteur autorise par le relais -> TRACKING_ACTIVE ✅",
							 decision.uid);
				if (_connectivityTask.mqttConnected()) {
					_telemetryService.forcePublish();
				}
			} else {
				transitionTo(types::SystemState::ALARM_STATE);
				_relayManager.submit({types::RelayCmd::CUT, types::RelaySource::AUTH, decision.timestamp});
				_telemetryService.setAuthentication(decision.result);
				_telemetryService.setActiveTracking(false);
				_alertManager.raise(types::AlertCode::AUTH_DENIED, types::AlertSeverity::MEDIUM, decision.timestamp);
				_logger.warn(logging::TAG_SYSTEM_CONTROLLER,
							 "CHAUFFEUR NON RECONNU (UID: %s). Moteur coupe -> ALARM_STATE 🔊 (Buzzer 60s, LED rouge rapide)",
							 decision.uid);
				if (_connectivityTask.mqttConnected()) {
					_telemetryService.forcePublish();
				}
			}
		}
	} else if (_systemState == types::SystemState::ALARM_STATE) {
		_relayManager.submit({types::RelayCmd::CUT, types::RelaySource::AUTH, utils::Timer::millis()});
		_telemetryService.setActiveTracking(false);

		// Bon badge présenté pendant l'alarme ?
		if (hasNewDecision) {
			_lastAuthenticationTimestamp = decision.timestamp;
			const bool isAuthorized = (decision.result == types::AuthenticationResult::AUTHORIZED);
			if (isAuthorized &&
				!localAuthenticationMayStartEngine(_systemState, _killLocked)) {
				transitionTo(types::SystemState::ENGINE_KILL);
				_relayManager.submit({types::RelayCmd::EMERGENCY_CUT,
									  types::RelaySource::REMOTE_COMMAND,
									  decision.timestamp});
				_telemetryService.setActiveTracking(false);
				_logger.warn(logging::TAG_SYSTEM_CONTROLLER,
							 "badge autorise refuse: verrou KILL actif jusqu'au redemarrage");
			} else if (isAuthorized) {
				_logger.info(logging::TAG_SYSTEM_CONTROLLER,
							 "BON BADGE PRESENTE PENDANT L'ALARME (UID: %s) -> Passage a TRACKING_ACTIVE ✅",
							 decision.uid);
				_alertManager.stopAlarm();
				transitionTo(types::SystemState::TRACKING_ACTIVE);
				_relayManager.submit({types::RelayCmd::ENGAGE, types::RelaySource::AUTH, decision.timestamp});
				_telemetryService.setAuthentication(decision.result);
				_telemetryService.setActiveTracking(true);
				_alertManager.setPattern(managers::AlertPattern::TRACKING_OK);
				if (_connectivityTask.mqttConnected()) {
					_telemetryService.forcePublish();
				}
			}
		}

		// Fin des 60s sans bon badge ?
		if (_alertManager.isAlarmTimedOut()) {
			_logger.info(logging::TAG_SYSTEM_CONTROLLER,
						 "Fin d'alarme (60s) sans badge valide -> Retour a WAIT_BADGE");
			_alertManager.stopAlarm();
			transitionTo(types::SystemState::WAIT_BADGE);
		}
	} else if (_systemState == types::SystemState::TRACKING_ACTIVE) {
		if (!_killLocked) {
			_relayManager.submit({types::RelayCmd::ENGAGE, types::RelaySource::AUTH, utils::Timer::millis()});
		}

		// Retrait de badge / coupure de contact
		if (hasNewDecision) {
			_lastAuthenticationTimestamp = decision.timestamp;
			_logger.info(logging::TAG_SYSTEM_CONTROLLER,
						 "BADGE PRESENTE EN SUIVI ACTIF (UID: %s) -> Coupure moteur -> WAIT_BADGE",
						 decision.uid);
			_relayManager.submit({types::RelayCmd::CUT, types::RelaySource::AUTH, decision.timestamp});
			_telemetryService.setActiveTracking(false);
			transitionTo(types::SystemState::WAIT_BADGE);
		}
	} else if (_systemState == types::SystemState::SOS_STATE) {
		_relayManager.submit({types::RelayCmd::CUT, types::RelaySource::BUTTON_SOS, utils::Timer::millis()});
		_telemetryService.setActiveTracking(false);
	} else if (_systemState == types::SystemState::ENGINE_KILL) {
		_relayManager.submit({types::RelayCmd::EMERGENCY_CUT, types::RelaySource::REMOTE_COMMAND, utils::Timer::millis()});
		_telemetryService.setActiveTracking(false);
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
	const uint64_t statusNowMs = utils::Timer::millis();
	const bool mqttConnected = _connectivityTask.mqttConnected();
	if (mqttConnected && !_lastMqttConnected) {
		_telemetryService.forcePublish();
	}
	_lastMqttConnected = mqttConnected;
	if (_lastStatusPublishMs == 0U ||
		statusNowMs - _lastStatusPublishMs >= 60000U) {
		_telemetryService.publishStatus(
			mqttConnected,
			_connectivityTask.networkConnected(),
			_gpsService.isRunning(), _sdDriver.isHealthy());
		_lastStatusPublishMs = statusNowMs;
	}

	_alertManager.update();
	_alertManager.process();
	processAlerts();

	_powerMonitor.update();
	_powerManager.update();
	logBatteryStatus();

	_relayManager.update();
	_relayManager.process();
	const types::RelayState relayState = _relayManager.state();
	if (!_relayStateKnown || relayState != _lastPublishedRelayState) {
		_telemetryService.publishEngine(
			relayState, _telemetryService.isActiveTracking());
		_lastPublishedRelayState = relayState;
		_relayStateKnown = true;
	}

	_fuelManager.update();
	_fuelManager.process();

	_fuelDriver.poll();
	_rtcDriver.poll();
	_sdDriver.poll();

	_diagnosticsService.update();
	_telemetryService.update();
	Serial.printf("[PERF][SYSTEM] update END duration=%llu\n",
				  static_cast<unsigned long long>(utils::Timer::millis() - updateStartMs));
}

void SystemController::processAlerts()
{
	if (!_alertManager.hasPendingAlert()) return;
	const types::AlertEvent alert = _alertManager.pendingAlert();
	if (!_hasProcessedAlert || alert.timestamp != _lastAlertTimestamp || alert.code != _lastAlertCode) {
		_hasProcessedAlert = true;
		_lastAlertTimestamp = alert.timestamp;
		_lastAlertCode = alert.code;
		_telemetryService.publishAlarm(alert);
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
		if (_connectivityTask.mqttConnected()) {
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
	_remoteRelayCommandMailbox.reset();
	_systemEventMailbox.reset();
	_relayManager.submit({types::RelayCmd::CUT, types::RelaySource::AUTH,
						  utils::Timer::millis()});
	_relayManager.process();
	_relayDriver.cut();
	_connectivityTask.stop();
	_rfidService.stop();
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
	_logger.info(logging::TAG_SYSTEM_CONTROLLER, "boot stage=%u: %s",
				 static_cast<unsigned int>(stage), message);
}

bool SystemController::failBoot(const char* message, constants::ErrorCode error)
{
	_bootStage = BootStage::Failed;
	_errorHandler.report(error);
	_logger.error(logging::TAG_SYSTEM_CONTROLLER, "%s", message);
	return false;
}

void SystemController::logGpsPosition()
{
	constexpr uint64_t gpsLogIntervalMs = 10000U;
	constexpr uint64_t gpsStaleAfterMs = 25000U;
	const uint64_t nowMs = utils::Timer::millis();
	if (nowMs - _lastGpsLogMs < gpsLogIntervalMs) return;
	_lastGpsLogMs = nowMs;

	const types::GpsData gps = _gpsService.data();
	if (!gps.hasLastFix) {
		_logger.info(logging::TAG_TRACKING,
					 "GPS (10s): attente fix, mode=%u satellites=%u HDOP=%.2f PDOP=%.2f",
					 static_cast<unsigned int>(gps.fixMode),
					 static_cast<unsigned int>(gps.satellites), gps.hdop, gps.pdop);
		return;
	}

	const uint64_t ageMs = nowMs - gps.timestamp;
	if (!gps.fixValid) {
		_logger.info(logging::TAG_TRACKING,
					 "GPS (10s): dernier fix expire, age=%llums lat=%.6f lon=%.6f alt=%.0fm sat=%u HDOP=%.1f PDOP=%.1f",
					 static_cast<unsigned long long>(ageMs), gps.latitude,
					 gps.longitude, gps.altitudeMeters,
					 static_cast<unsigned int>(gps.satellites), gps.hdop, gps.pdop);
		return;
	}
	if (ageMs > gpsStaleAfterMs) {
		_logger.info(logging::TAG_TRACKING,
					 "GPS (10s): dernier fix connu, age=%llums lat=%.6f lon=%.6f alt=%.0fm vitesse=%.1fkm/h cap=%.0f sat=%u HDOP=%.1f PDOP=%.1f",
					 static_cast<unsigned long long>(ageMs), gps.latitude,
					 gps.longitude, gps.altitudeMeters, gps.speedKmh,
					 gps.headingDeg, static_cast<unsigned int>(gps.satellites),
					 gps.hdop, gps.pdop);
		return;
	}

	_logger.info(logging::TAG_TRACKING,
				 "GPS (10s): fix=%u lat=%.6f lon=%.6f alt=%.0fm vitesse=%.1fkm/h cap=%.0f sat=%u HDOP=%.1f PDOP=%.1f",
				 static_cast<unsigned int>(gps.fixMode), gps.latitude,
				 gps.longitude, gps.altitudeMeters, gps.speedKmh, gps.headingDeg,
				 static_cast<unsigned int>(gps.satellites), gps.hdop, gps.pdop);
}

types::SystemState SystemController::systemState() const
{
	return _systemState;
}

bool SystemController::submitRemoteRelayCommand(types::RelayCmd command,
												uint64_t timestamp)
{
	if (!_initialized || !_remoteRelayCommandMailbox.ready()) return false;
	if (command != types::RelayCmd::CUT &&
		command != types::RelayCmd::EMERGENCY_CUT &&
		command != types::RelayCmd::ENGAGE) {
		return false;
	}
	return _remoteRelayCommandMailbox.submit(
		{command, types::RelaySource::REMOTE_COMMAND, timestamp});
}

void SystemController::processRemoteRelayCommands()
{
	types::RelayCommand command{};
	while (_remoteRelayCommandMailbox.receive(command)) {
		switch (command.cmd) {
			case types::RelayCmd::CUT:
				if (stateAfterRemoteCut(_systemState, _killLocked) ==
					types::SystemState::ENGINE_KILL) {
					_killLocked = true;
					(void)_relayManager.submit({
						types::RelayCmd::EMERGENCY_CUT,
						types::RelaySource::REMOTE_COMMAND, command.timestamp});
					_telemetryService.setActiveTracking(false);
					break;
				}
				_logger.info(logging::TAG_SYSTEM_CONTROLLER,
							 "commande distante STOP traitee par la tache de controle");
				(void)_relayManager.submit(command);
				_telemetryService.setActiveTracking(false);
				if (_systemState == types::SystemState::SOS_STATE) {
					_alertManager.setPattern(managers::AlertPattern::SOS);
				} else {
					transitionTo(types::SystemState::WAIT_BADGE);
					_alertManager.setPattern(managers::AlertPattern::WAIT_BADGE);
				}
				break;

			case types::RelayCmd::EMERGENCY_CUT:
				_killLocked = true;
				_authenticationService.clearAuthentication();
				_logger.warn(logging::TAG_SYSTEM_CONTROLLER,
							 "commande distante KILL appliquee par la tache de controle; relais verrouille");
				transitionTo(types::SystemState::ENGINE_KILL);
				(void)_relayManager.submit(command);
				_telemetryService.setActiveTracking(false);
				break;

			case types::RelayCmd::ENGAGE: {
				const bool localAuthenticationActive =
					_authenticationService.decision().state ==
					types::AuthState::AUTHENTICATED;
				if (!remoteEngineOnMayBeApplied(
						_systemState, _killLocked,
						localAuthenticationActive)) {
					_logger.warn(logging::TAG_SYSTEM_CONTROLLER,
								 "commande distante ENGINE_ON refusee par la tache de controle (authentification ou verrou KILL)");
					break;
				}
				if (_relayManager.submit(command)) {
					transitionTo(types::SystemState::TRACKING_ACTIVE);
					_telemetryService.setActiveTracking(true);
					_alertManager.setPattern(managers::AlertPattern::TRACKING_OK);
				}
				break;
			}
		}
	}
}

void SystemController::transitionTo(types::SystemState newState)
{
	if (_systemState == newState) return;
	_logger.info(logging::TAG_SYSTEM_CONTROLLER,
				 "FSM TRANSITION : etat %u -> %u",
				 static_cast<unsigned int>(_systemState),
				 static_cast<unsigned int>(newState));
	_systemState = newState;
}

void SystemController::onEvent(const types::Event& event)
{
	if (!_systemEventMailbox.submit(event)) {
		_logger.warn(logging::TAG_SYSTEM_CONTROLLER,
					 "mailbox des evenements de controle pleine; evenement=%u non mis en file",
					 static_cast<unsigned int>(event.id));
	}
}

void SystemController::processSystemEvents()
{
	types::Event event{};
	while (_systemEventMailbox.receive(event)) {
		processEvent(event);
	}
}

void SystemController::processEvent(const types::Event& event)
{
	switch (event.id) {
		case constants::EventID::SOS_BUTTON_PRESSED:
			_logger.warn(logging::TAG_SYSTEM_CONTROLLER,
						 "EVENEMENT : Bouton SOS presse -> Passage a SOS_STATE 🆘");
			transitionTo(types::SystemState::SOS_STATE);
			_relayManager.submit({types::RelayCmd::CUT, types::RelaySource::BUTTON_SOS, event.timestamp});
			_telemetryService.setActiveTracking(false);
			_alertManager.setPattern(managers::AlertPattern::SOS);
			if (_connectivityTask.mqttConnected()) {
				_telemetryService.forcePublish();
			}
			break;

		case constants::EventID::REMOTE_STOP_COMMAND:
			if (!submitRemoteRelayCommand(types::RelayCmd::CUT,
										  event.timestamp)) {
				_logger.warn(logging::TAG_SYSTEM_CONTROLLER,
							 "mailbox commandes relais pleine; STOP distant non mis en file");
			}
			break;

		case constants::EventID::REMOTE_KILL_COMMAND:
			if (!submitRemoteRelayCommand(types::RelayCmd::EMERGENCY_CUT,
										  event.timestamp)) {
				_logger.warn(logging::TAG_SYSTEM_CONTROLLER,
							 "mailbox commandes relais pleine; KILL distant non mis en file");
			}
			break;

		case constants::EventID::SOS_ACK_RECEIVED:
			_logger.info(logging::TAG_SYSTEM_CONTROLLER,
						 "EVENEMENT : Commande MQTT SOS_ACK -> Desactivation SOS");
			if (_killLocked) {
				transitionTo(types::SystemState::ENGINE_KILL);
			} else if (_systemState == types::SystemState::SOS_STATE) {
				_alertManager.clear();
				transitionTo(types::SystemState::WAIT_BADGE);
			}
			break;

		case constants::EventID::NETWORK_LOST:
			_logger.warn(logging::TAG_SYSTEM_CONTROLLER,
						 "EVENEMENT : Perte reseau -> Mode OFFLINE active");
			_connectivityTask.requestReconnect();
			break;

		case constants::EventID::NETWORK_CONNECTED:
			_logger.info(logging::TAG_SYSTEM_CONTROLLER,
						 "EVENEMENT : Reseau connecte");
			break;

		case constants::EventID::BATTERY_LOW:
			_logger.warn(logging::TAG_SYSTEM_CONTROLLER,
						 "EVENEMENT : Batterie < 10%% critique");
			break;

		case constants::EventID::FUEL_ANOMALY:
			_logger.warn(logging::TAG_SYSTEM_CONTROLLER,
						 "EVENEMENT : Anomalie carburant (vol suspecte)");
			break;

		case constants::EventID::BADGE_REMOVED:
			if (_systemState == types::SystemState::TRACKING_ACTIVE) {
				_logger.info(logging::TAG_SYSTEM_CONTROLLER,
							 "EVENEMENT : Retrait badge -> WAIT_BADGE");
				_relayManager.submit({types::RelayCmd::CUT, types::RelaySource::AUTH, event.timestamp});
				_telemetryService.setActiveTracking(false);
				transitionTo(types::SystemState::WAIT_BADGE);
			}
			break;

		default:
			break;
	}
}

}  // namespace app
}  // namespace fortico
