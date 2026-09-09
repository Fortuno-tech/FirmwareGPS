#pragma once

#include "core/logger/logger.h"
#include "drivers/sim7600/sim7600_driver.h"
#include "middleware/mqtt_client/mqtt_client.h"
#include "types/mqtt_types.h"

namespace fortico {
namespace middleware {

class CellularMqttClient final : public MqttClient {
public:
    CellularMqttClient(drivers::Sim7600Driver& modem, core::Logger& logger);
    bool setCredentials(const types::MqttCredentials& credentials);
    bool connect(const char* host, uint16_t port) override;
    bool publish(const char* topic, const void* payload,
                 std::size_t length) override;
    void disconnect() override;
    bool connected() const;

private:
    void logMqttFailure(const char* step, const char* response,
                        const char* urcPrefix) const;
    void releaseResources();

    drivers::Sim7600Driver& _modem;
	core::Logger& _logger;
    types::MqttCredentials _credentials;
    bool _credentialsSet;
    bool _connected;
	bool _serviceStarted;
	bool _clientAcquired;
};

}  // namespace middleware
}  // namespace fortico
