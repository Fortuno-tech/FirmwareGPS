#include <Arduino.h>

#include "app/application/application.h"
#include "core/event_bus/event_bus.h"
#include "core/diagnostics/diagnostics.h"
#include "core/error_handler/error_handler.h"
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
#include "hal/gpio/gpio_hal.h"
#include "hal/i2c/i2c_hal.h"
#include "hal/uart/uart_hal.h"
#include "hal/nvs/nvs_hal.h"
#include "hal/spi/spi_hal.h"
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
#include "services/telemetry/telemetry_service.h"
#include "managers/power_manager/power_manager.h"
#include "managers/ota_manager/ota_manager.h"
#include "services/rfid/rfid_service.h"

namespace {

fortico::hal::GpioHal g_gpioHal;
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
fortico::drivers::Sim7600Driver g_sim7600(g_uartHal);

fortico::core::EventBus g_eventBus;
fortico::core::MemoryMonitor g_memoryMonitor;
fortico::core::Diagnostics g_diagnostics(g_memoryMonitor);
fortico::core::PowerMonitor g_powerMonitor(g_battery);
fortico::interfaces::IDriver* g_powerDrivers[] = {&g_battery};
fortico::managers::PowerManager g_powerManager(
    g_powerMonitor, g_powerDrivers, sizeof(g_powerDrivers) / sizeof(g_powerDrivers[0]));
fortico::managers::EspOtaBackend g_otaBackend;
fortico::managers::OtaManager g_otaManager(g_otaBackend);
fortico::managers::RelayManager g_relayManager(g_relayDriver);
fortico::managers::FuelManager g_fuelManager(g_fuelDriver);
fortico::core::ErrorHandler g_errorHandler(&g_eventBus);
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
    &g_battery, &g_rfidDriver, &g_sim7600, &g_relayDriver,
    &g_fuelDriver, &g_rtcDriver, &g_sdDriver};
fortico::managers::DiagnosticsManager g_diagnosticsManager(
    g_diagnosticDrivers, sizeof(g_diagnosticDrivers) / sizeof(g_diagnosticDrivers[0]));
fortico::services::DiagnosticsService g_diagnosticsService(
    g_diagnosticsManager, g_diagnostics, &g_eventBus);
fortico::managers::TelemetryManager g_telemetryManager(g_battery, &g_eventBus);
fortico::services::TelemetryService g_telemetryService(g_telemetryManager);
fortico::app::Application g_application(
    g_eventBus,
    g_alertManager,
    g_authenticationService,
    g_rfidService,
    g_sim7600,
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
    g_sdDriver);

bool g_started = false;

}  // namespace

void setup()
{
    Serial.begin(115200);
    delay(250);

    if (g_nvsDriver.init() != fortico::constants::ErrorCode::OK) {
        return;
    }

    g_authenticationManager.setCredentialStore(&g_storageService);

    if (!g_application.init()) {
        return;
    }
    g_started = g_application.start();
}

void loop()
{
    (void)g_started;
    delay(10);
}
