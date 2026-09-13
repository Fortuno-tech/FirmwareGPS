#pragma once

#include "core/logger/logger.h"
#include "drivers/rtc/rtc_driver.h"
#include "drivers/sd/sd_driver.h"
#include "drivers/sim7600/sim7600_driver.h"
#include "managers/relay_manager/relay_manager.h"
#include "middleware/mqtt_client/cellular_mqtt_client.h"
#include "services/gps/gps_service.h"
#include "services/mqtt/mqtt_publish_queue.h"
#include "services/sim7600/sim7600_service.h"
#include "services/telemetry/telemetry_service.h"

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
                       core::Logger& logger);

    void handle(const drivers::MqttIncomingMessage& message);

private:
    bool extractString(const char* json, std::size_t length,
                       const char* key, char* output,
                       std::size_t outputSize) const;
    bool queueAck(const char* id, const char* command, bool success,
                  const char* error);

    MqttPublishQueue& _queue;
    managers::RelayManager& _relayManager;
    TelemetryService& _telemetryService;
    const GpsService& _gpsService;
    const Sim7600Service& _simService;
    const drivers::SdDriver& _sdDriver;
    const middleware::CellularMqttClient& _mqttClient;
    const drivers::RtcDriver& _rtcDriver;
    core::Logger& _logger;
};

}  // namespace services
}  // namespace fortico
