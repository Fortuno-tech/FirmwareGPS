#pragma once

#include "core/logger/logger.h"
#include "drivers/rtc/rtc_driver.h"
#include "drivers/sd/sd_driver.h"
#include "drivers/sim7600/sim7600_driver.h"
#include "managers/relay_manager/relay_manager.h"
#include "middleware/mqtt_client/cellular_mqtt_client.h"
#include "core/event_bus/event_bus.h"
#include "services/gps/gps_service.h"
#include "services/mqtt/mqtt_publish_queue.h"
#include "services/mqtt/mqtt_command_security.h"
#include "services/security/security_service.h"
#include "services/sim7600/sim7600_service.h"
#include "services/storage/storage_service.h"
#include "services/telemetry/telemetry_service.h"
#include "services/authentication/authentication_service.h"

namespace fortico {
namespace services {

class MqttCommandHandler final {
public:
    MqttCommandHandler(MqttPublishQueue& queue,
                       managers::RelayManager& relayManager,
                       TelemetryService& telemetryService,
                       const GpsService& gpsService,
                       const Sim7600Service& simService,
                       const drivers::SdDriver& sdDriver,
                       const middleware::CellularMqttClient& mqttClient,
                       const drivers::RtcDriver& rtcDriver,
                       core::Logger& logger,
                       core::EventBus* eventBus = nullptr);
    MqttCommandHandler(MqttPublishQueue& queue,
                       managers::RelayManager& relayManager,
                       TelemetryService& telemetryService,
                       const GpsService& gpsService,
                       const Sim7600Service& simService,
                       const drivers::SdDriver& sdDriver,
                       const middleware::CellularMqttClient& mqttClient,
                       const drivers::RtcDriver& rtcDriver,
                       SecurityService& securityService,
                       MqttCommandSecurity& commandSecurity,
                       StorageService& storageService,
                       AuthenticationService& authenticationService,
                       core::Logger& logger,
                       core::EventBus* eventBus = nullptr);

    void setEventBus(core::EventBus* eventBus);
    void handle(const drivers::MqttIncomingMessage& message);

private:
    bool queueReceiptAck(const char* id, const char* command, bool accepted,
                         const char* error);
    void audit(const MqttCommandRequest& request, bool accepted,
               const char* reason, uint64_t timestamp);

    MqttPublishQueue& _queue;
    managers::RelayManager& _relayManager;
    TelemetryService& _telemetryService;
    const GpsService& _gpsService;
    const Sim7600Service& _simService;
    const drivers::SdDriver& _sdDriver;
    const middleware::CellularMqttClient& _mqttClient;
    const drivers::RtcDriver& _rtcDriver;
    SecurityService* _securityService;
    MqttCommandSecurity* _commandSecurity;
    StorageService* _storageService;
    AuthenticationService* _authenticationService;
    MqttCommandLatch _commandLatch;
    core::Logger& _logger;
    core::EventBus* _eventBus;
};

}  // namespace services
}  // namespace fortico
