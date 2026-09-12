#include <Arduino.h>

#include <cctype>

#include "app/application/application.h"
#include "core/event_bus/event_bus.h"
#include "core/diagnostics/diagnostics.h"
#include "core/error_handler/error_handler.h"
#include "core/logger/logger.h"
#include "core/logger/composite_log_sink.h"
#include "core/memory/memory_monitor.h"
#include "core/power/power_monitor.h"
#include "drivers/battery/battery_driver.h"
#include "drivers/nvs/nvs_driver.h"
#include "drivers/buzzer/buzzer_driver.h"
#include "drivers/button/button_driver.h"
#include "drivers/led/led_driver.h"
#include "drivers/relay/relay_driver.h"
#include "drivers/fuel/fuel_driver.h"
#include "drivers/rtc/rtc_driver.h"
#include "drivers/sd/sd_driver.h"
#include "drivers/rfid/rfid_driver.h"
#include "drivers/sim7600/sim7600_driver.h"
#include "drivers/sim7600/sim7600_power_driver.h"
#include "drivers/gps/sim7600_gnss_driver.h"
#include "middleware/mqtt_client/cellular_mqtt_client.h"
#include "hal/gpio/gpio_hal.h"
#include "hal/delay/delay_hal.h"
#include "hal/i2c/i2c_hal.h"
#include "hal/uart/uart_hal.h"
#include "hal/nvs/nvs_hal.h"
#include "hal/spi/spi_hal.h"
#include "hal/serial/serial_log_sink.h"
#include "managers/alert_manager/alert_manager.hpp"
#include "managers/relay_manager/relay_manager.h"
#include "managers/fuel_manager/fuel_manager.h"
#include "managers/authentication_manager/authentication_manager.hpp"
#include "managers/diagnostics_manager/diagnostics_manager.hpp"
#include "managers/telemetry_manager/telemetry_manager.h"
#include "services/authentication/authentication_service.h"
#include "services/diagnostics/diagnostics_service.h"
#include "services/security/security_service.h"
#include "services/storage/storage_service.h"
#include "services/storage/sd_log_sink.h"
#include "services/telemetry/telemetry_service.h"
#include "services/mqtt/mqtt_telemetry_sink.h"
#include "services/mqtt/mqtt_identity.h"
#include "services/temperature/temperature_sensor.h"
#include "services/mqtt/mqtt_publish_task.h"
#include "managers/power_manager/power_manager.h"
#include "managers/ota_manager/ota_manager.h"
#include "services/rfid/rfid_service.h"
#include "services/sim7600/sim7600_service.h"
#include "services/gps/gps_service.h"
#include "services/security/provisioning_service.h"
#include "services/security/ota_key_store.h"
#include "config/pin_config.h"
#include "config/hardware_config.h"
#include "config/security_config.h"

namespace {

fortico::hal::GpioHal g_gpioHal;
fortico::hal::DelayHal g_delayHal;
fortico::hal::I2cHal g_i2cHal;
fortico::hal::UartHal g_uartHal;
fortico::hal::AdcHal g_adcHal;
fortico::hal::NvsHal g_nvsHal;
fortico::hal::SpiHal g_spiHal;

fortico::drivers::BatteryDriver g_battery(g_adcHal);
fortico::drivers::NvsDriver g_nvsDriver(g_nvsHal);
fortico::drivers::BuzzerDriver g_buzzer(g_gpioHal);
fortico::drivers::ButtonDriver g_button(g_gpioHal);
fortico::drivers::LedDriver g_led(g_gpioHal);
fortico::drivers::RelayDriver g_relayDriver(g_gpioHal);
fortico::drivers::FuelDriver g_fuelDriver(g_adcHal);
fortico::drivers::RtcDriver g_rtcDriver(g_i2cHal);
fortico::drivers::SdDriver g_sdDriver(g_spiHal);
fortico::drivers::RfidDriver g_rfidDriver(g_i2cHal);
fortico::drivers::Sim7600Driver g_sim7600(g_uartHal, g_delayHal);
fortico::drivers::Sim7600PowerDriver g_sim7600PowerDriver(g_gpioHal, g_delayHal);
fortico::services::Sim7600Service g_sim7600Service(g_sim7600);
fortico::drivers::Sim7600GnssDriver g_gpsDriver(g_sim7600);
fortico::services::GpsService g_gpsService(g_gpsDriver);

fortico::core::EventBus g_eventBus;
fortico::core::MemoryMonitor g_memoryMonitor;
fortico::core::Diagnostics g_diagnostics(g_memoryMonitor);
fortico::core::PowerMonitor g_powerMonitor(g_battery);
fortico::interfaces::IDriver* g_powerDrivers[] = {&g_battery};
fortico::managers::PowerManager g_powerManager(
    g_powerMonitor, g_powerDrivers, sizeof(g_powerDrivers) / sizeof(g_powerDrivers[0]));
fortico::managers::EspOtaBackend g_otaBackend;
fortico::managers::Ed25519OtaVerifier g_otaVerifier(nullptr);
fortico::managers::OtaManager g_otaManager(g_otaBackend, g_otaVerifier);
fortico::managers::RelayManager g_relayManager(g_relayDriver);
fortico::managers::FuelManager g_fuelManager(g_fuelDriver);
fortico::core::ErrorHandler g_errorHandler(&g_eventBus);
fortico::hal::SerialLogSink g_serialLogSink;
fortico::services::SdLogSink g_sdLogSink(g_sdDriver);
fortico::core::CompositeLogSink g_logSink(g_serialLogSink, g_sdLogSink);
fortico::core::Logger g_logger;
fortico::middleware::CellularMqttClient g_mqttClient(g_sim7600, g_logger);
fortico::services::MqttPublishQueue g_mqttPublishQueue(g_logger);
fortico::services::MqttPublishTask g_mqttPublishTask(
    g_mqttPublishQueue, g_mqttClient, g_logger);
fortico::services::UnavailableTemperatureSensor g_temperatureSensor;
fortico::services::SecurityService g_securityService(g_nvsDriver);
fortico::services::StorageService g_storageService(
    g_nvsDriver, g_securityService, &g_sdDriver);
fortico::managers::AuthenticationManager g_authenticationManager(&g_eventBus);
fortico::services::AuthenticationService g_authenticationService(
    g_authenticationManager);
fortico::managers::AlertManager g_alertManager(g_button, g_buzzer, g_led);
fortico::services::RfidService g_rfidService(
    g_rfidDriver, g_authenticationService, g_eventBus);
fortico::interfaces::IDriver* g_diagnosticDrivers[] = {
    &g_battery, &g_rfidDriver, &g_sim7600PowerDriver, &g_sim7600, &g_relayDriver,
    &g_fuelDriver, &g_rtcDriver, &g_sdDriver};
fortico::managers::DiagnosticsManager g_diagnosticsManager(
    g_diagnosticDrivers, sizeof(g_diagnosticDrivers) / sizeof(g_diagnosticDrivers[0]));
fortico::services::DiagnosticsService g_diagnosticsService(
    g_diagnosticsManager, g_diagnostics, &g_eventBus);
fortico::managers::TelemetryManager g_telemetryManager(g_battery, &g_eventBus);
fortico::services::MqttTelemetrySink g_telemetrySink(
     g_mqttPublishQueue, fortico::services::MqttIdentity::VehicleId);
fortico::services::TelemetryService g_telemetryService(
     g_telemetryManager, &g_telemetrySink, &g_gpsService, &g_rtcDriver,
     &g_temperatureSensor);
fortico::app::Application g_application(
    g_eventBus,
    g_logger,
    g_alertManager,
    g_authenticationService,
    g_rfidService,
    g_sim7600PowerDriver,
    g_sim7600Service,
    g_gpsService,
    g_diagnosticsService,
    g_powerMonitor,
    g_errorHandler,
    g_securityService,
    g_storageService,
    g_telemetryService,
    g_powerManager,
    g_otaManager,
    g_relayDriver,
    g_relayManager,
    g_fuelDriver,
    g_fuelManager,
    g_rtcDriver,
    g_sdDriver,
    g_mqttClient);

bool g_started = false;

constexpr const char* kDefaultRfidUid = "91:51:E2:06";

bool isValidRfidUid(const String& uid)
{
    // PN532 ISO14443-A UIDs have 4, 7, or 10 bytes and use XX:XX:... notation.
    const size_t length = uid.length();
    const size_t byteCount = (length + 1U) / 3U;
    if ((byteCount != 4U && byteCount != 7U && byteCount != 10U) ||
        length != byteCount * 3U - 1U) {
        return false;
    }

    for (size_t index = 0U; index < length; ++index) {
        if ((index + 1U) % 3U == 0U) {
            if (uid[index] != ':') return false;
        } else if (!isxdigit(static_cast<unsigned char>(uid[index]))) {
            return false;
        }
    }
    return true;
}

bool seedDefaultRfidBadge()
{
    uint32_t seeded = 0U;
    if (!g_nvsDriver.open(fortico::security::NVS_NAMESPACE, true)) {
        return false;
    }
    const bool markerExists = g_nvsDriver.getUInt32(
        fortico::security::NVS_KEY_RFID_DEFAULT_SEEDED, seeded);
    g_nvsDriver.close();
    if (markerExists && seeded == 1U) return true;

    const bool badgeReady =
        g_storageService.isBadgeAuthorized(kDefaultRfidUid) ||
        g_storageService.addBadge(kDefaultRfidUid);
    if (!badgeReady || !g_nvsDriver.open(fortico::security::NVS_NAMESPACE, false)) {
        return false;
    }
    const bool markerSaved = g_nvsDriver.putUInt32(
        fortico::security::NVS_KEY_RFID_DEFAULT_SEEDED, 1U);
    g_nvsDriver.close();
    return markerSaved;
}

}  // namespace

void handleSerialProvisioning(
    fortico::services::ProvisioningService& provisioningService,
    fortico::services::StorageService& storageService)
{
    const bool alreadyProvisioned = provisioningService.isProvisioned();
    const uint32_t timeoutMs = alreadyProvisioned ? 5000U : 30000U;

    if (!alreadyProvisioned) {
        Serial.println(F("[PROV] Device unprovisioned. Waiting 30s for PROV payload on Serial..."));
    } else {
        Serial.println(F("[PROV] Device provisioned. Waiting 5s for serial commands..."));
    }
    Serial.println(F("PROV:READY"));

    String line = "";
    line.reserve(3000);

    uint32_t lastActivityMs = millis();
    while (millis() - lastActivityMs < timeoutMs) {
        while (Serial.available() > 0) {
            const char c = static_cast<char>(Serial.read());
            if (c == '\n' || c == '\r') {
                line.trim();
                if (line.length() > 0) {
                    lastActivityMs = millis();

                    if (line.startsWith("PROV:RFID:")) {
                        // Most serial monitors do not echo transmitted text.
                        // Echo RFID administration commands only; never echo
                        // provisioning credentials or certificate payloads.
                        Serial.print(F("PROV:RFID:COMMAND "));
                        Serial.println(line);
                        // Physical UART administration is deliberately limited
                        // to this short provisioning window on an already
                        // provisioned device.
                        const bool add = line.startsWith("PROV:RFID:ADD:");
                        const bool remove = line.startsWith("PROV:RFID:REMOVE:");
                        const int uidStart = add ? 14 : (remove ? 17 : -1);
                        String uid = uidStart >= 0 ? line.substring(uidStart) : "";
                        uid.toUpperCase();

                        if (!alreadyProvisioned || !storageService.isRunning() ||
                            !isValidRfidUid(uid)) {
                            Serial.println(F("PROV:RFID:FAIL"));
                        } else if (add) {
                            if (storageService.isBadgeAuthorized(uid.c_str())) {
                                Serial.println(F("PROV:RFID:ALREADY_PRESENT"));
                            } else if (storageService.addBadge(uid.c_str())) {
                                Serial.println(F("PROV:RFID:ADDED"));
                            } else {
                                Serial.println(F("PROV:RFID:FAIL"));
                            }
                        } else if (remove && storageService.removeBadge(uid.c_str())) {
                            Serial.println(F("PROV:RFID:REMOVED"));
                        } else {
                            Serial.println(F("PROV:RFID:FAIL"));
                        }
                    } else if (line == "PROV:CERT:CHECK") {
                        if (provisioningService.hasModemCertificate("hivemq-ca.pem")) {
                            Serial.println(F("PROV:CERT:PRESENT"));
                        } else {
                            Serial.println(F("PROV:CERT:ABSENT"));
                        }
                    } else if (line.startsWith("PROV:CERT:")) {
                        // Format: PROV:CERT:<expectedSize>:<base64Data>
                        int sepIdx = line.indexOf(':', 10);
                        if (sepIdx > 10) {
                            String sizeStr = line.substring(10, sepIdx);
                            String base64Str = line.substring(sepIdx + 1);
                            size_t expectedSize = static_cast<size_t>(sizeStr.toInt());
                            Serial.printf("[PROV] Received CERT: expSize=%u, b64Len=%u\n",
                                          static_cast<unsigned int>(expectedSize),
                                          static_cast<unsigned int>(base64Str.length()));
                            if (provisioningService.installModemCertificate("hivemq-ca.pem", expectedSize, base64Str.c_str())) {
                                Serial.println(F("PROV:CERT:OK"));
                            } else {
                                Serial.println(F("PROV:CERT:FAIL"));
                            }
                        } else {
                            Serial.println(F("PROV:CERT:FAIL"));
                        }
                    } else if (line.startsWith("PROV:")) {
                        // Parse PROV:<rootKeyHex>:<otaPubKeyHex>:<deviceId>:<clientId>:<user>:<pass>
                        int idx1 = line.indexOf(':', 5);
                        int idx2 = line.indexOf(':', idx1 + 1);
                        int idx3 = line.indexOf(':', idx2 + 1);
                        int idx4 = line.indexOf(':', idx3 + 1);
                        int idx5 = line.indexOf(':', idx4 + 1);

                        if (idx1 > 0 && idx2 > idx1 && idx3 > idx2 && idx4 > idx3 && idx5 > idx4) {
                            String rootKeyHex = line.substring(5, idx1);
                            String otaPubKeyHex = line.substring(idx1 + 1, idx2);
                            String deviceId = line.substring(idx2 + 1, idx3);
                            String clientId = line.substring(idx3 + 1, idx4);
                            String username = line.substring(idx4 + 1, idx5);
                            String password = line.substring(idx5 + 1);

                            uint8_t rootKey[32] = {};
                            uint8_t otaPubKey[32] = {};

                            if (fortico::services::ProvisioningService::hexToBytes(rootKeyHex.c_str(), rootKey, 32) &&
                                fortico::services::ProvisioningService::hexToBytes(otaPubKeyHex.c_str(), otaPubKey, 32)) {

                                fortico::types::MqttCredentials creds{};
                                strncpy(creds.clientId, clientId.c_str(), sizeof(creds.clientId) - 1);
                                strncpy(creds.username, username.c_str(), sizeof(creds.username) - 1);
                                strncpy(creds.password, password.c_str(), sizeof(creds.password) - 1);

                                if (provisioningService.provisionDevice(rootKey, 32, otaPubKey, 32, creds, deviceId.c_str())) {
                                    Serial.println(F("PROV:OK"));
                                    return;
                                }
                            }
                        }
                        Serial.println(F("PROV:FAIL"));
                    }
                }
                line = "";
            } else {
                line += c;
            }
        }
        g_delayHal.waitMs(10U);
    }
    Serial.println(F("[PROV] Provisioning timeout reached. Starting with current state."));
}

void setup()
{
    Serial.setRxBufferSize(4096);
    Serial.begin(115200);
    g_delayHal.waitMs(250U);

    if (!g_logger.init(fortico::logging::DEVELOPMENT_MIN_LEVEL,
                       &g_logSink) || !g_logger.start()) {
        Serial.println(F("[BOOT][ERROR] Logger initialization failed"));
        return;
    }
    g_logger.info(fortico::logging::TAG_SYSTEM_CONTROLLER,
                  "console de diagnostic active");

    uint8_t i2cScanAddrs[16] = {};
    size_t i2cScanCount = 0;
    if (g_i2cHal.init(fortico::pins::I2C_SDA, fortico::pins::I2C_SCL, fortico::hardware::I2C_FREQUENCY_HZ)) {
        if (g_i2cHal.scan(i2cScanAddrs, sizeof(i2cScanAddrs), &i2cScanCount)) {
            Serial.printf("[I2C] Scan bus (SDA=%d, SCL=%d) : %u peripherique(s) detecte(s)\n",
                          fortico::pins::I2C_SDA, fortico::pins::I2C_SCL, static_cast<unsigned int>(i2cScanCount));
            for (size_t i = 0; i < i2cScanCount; ++i) {
                if (i2cScanAddrs[i] == 0x68) {
                    Serial.println(F("[I2C]  -> 0x68 : RTC DS3231 [OK]"));
                } else if (i2cScanAddrs[i] == 0x24) {
                    Serial.println(F("[I2C]  -> 0x24 : RFID PN532 [OK]"));
                } else {
                    Serial.printf("[I2C]  -> 0x%02X : Peripherique detecte\n", i2cScanAddrs[i]);
                }
            }
            if (i2cScanCount == 0) {
                Serial.println(F("[I2C][WARN] Aucun composant detecte sur le bus I2C ! Verifiez le cablage et les pull-ups."));
            }
        }
    }

    if (g_nvsDriver.init() != fortico::constants::ErrorCode::OK) {
		g_logger.error(fortico::logging::TAG_SYSTEM_CONTROLLER,
					   "initialisation NVS impossible");
        return;
    }

    g_sim7600PowerDriver.init();
    g_sim7600PowerDriver.start();
    if (g_sim7600.init() == fortico::constants::ErrorCode::OK) {
        g_sim7600PowerDriver.setPowerOnIssued(true);
    } else {
        g_sim7600PowerDriver.powerOn();
        g_sim7600.init();
    }
    g_sim7600.start();

    fortico::services::ProvisioningService provisioningService(g_securityService, g_nvsDriver, &g_sim7600);
    const bool alreadyProvisioned = provisioningService.isProvisioned();
    if (alreadyProvisioned && (!g_securityService.start() || !g_storageService.start())) {
        g_logger.error(fortico::logging::TAG_SYSTEM_CONTROLLER,
                       "initialisation stockage RFID impossible");
        return;
    }
    if (alreadyProvisioned && !seedDefaultRfidBadge()) {
        g_logger.warn(fortico::logging::TAG_SYSTEM_CONTROLLER,
                      "ajout du badge RFID par defaut impossible");
    }
    handleSerialProvisioning(provisioningService, g_storageService);

    g_securityService.start();

    uint8_t otaPubKey[32] = {};
    fortico::services::OtaKeyStore otaKeyStore(g_nvsDriver);
    if (otaKeyStore.loadPublicKey(otaPubKey)) {
        g_otaVerifier = fortico::managers::Ed25519OtaVerifier(otaPubKey);
    }

    g_authenticationManager.setCredentialStore(&g_storageService);

    if (!g_application.init()) {
		g_logger.error(fortico::logging::TAG_SYSTEM_CONTROLLER,
					   "initialisation application impossible");
        return;
    }
    g_started = g_application.start();
	if (!g_started) {
		g_logger.error(fortico::logging::TAG_SYSTEM_CONTROLLER,
					   "demarrage application impossible");
    } else if (!g_mqttPublishTask.start()) {
        g_logger.error(fortico::logging::TAG_MQTT,
                       "demarrage MqttPublishTask impossible");
	}
}

void loop()
{
    (void)g_started;
    g_delayHal.waitMs(10U);
}
