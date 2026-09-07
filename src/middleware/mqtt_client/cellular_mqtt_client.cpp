#include "middleware/mqtt_client/cellular_mqtt_client.h"

#include <cstdio>
#include <cstring>

#include "config/sim7600_mqtt_config.h"

namespace fortico {
namespace middleware {

CellularMqttClient::CellularMqttClient(drivers::Sim7600Driver& modem)
    : _modem(modem), _credentials{}, _credentialsSet(false), _connected(false) {}

bool CellularMqttClient::setCredentials(
    const types::MqttCredentials& credentials)
{
    if (credentials.clientId[0] == '\0' || credentials.username[0] == '\0')
        return false;
    _credentials = credentials;
    _credentialsSet = true;
    return true;
}

bool CellularMqttClient::connect(const char* host, uint16_t port)
{
    if (!_credentialsSet || host == nullptr || !_modem.isHealthy()) return false;
    if (std::strlen(host) > 96U || std::strlen(_credentials.clientId) > 63U ||
        std::strlen(_credentials.username) > 63U ||
        std::strlen(_credentials.password) > 95U) return false;
    char response[256]{};
        if (!_modem.sendCommand(modem_mqtt::Commands::START, response,
                                sizeof(response), 5000U))
        return false;
    char command[384]{};
        std::snprintf(command, sizeof(command),
                      modem_mqtt::Commands::ACQUIRE_CLIENT,
                      _credentials.clientId);
    if (!_modem.sendCommand(command, response, sizeof(response), 3000U)) return false;
    std::snprintf(command, sizeof(command),
                      modem_mqtt::Commands::CONNECT, host,
                      static_cast<unsigned int>(port),
                      modem_mqtt::Commands::KEEPALIVE_SECONDS,
                      _credentials.username, _credentials.password);
    _connected = _modem.sendCommand(command, response, sizeof(response), 15000U);
    return _connected;
}

bool CellularMqttClient::publish(const char* topic, const void* payload,
                                 std::size_t length)
{
    if (!_connected || topic == nullptr || payload == nullptr || length == 0U)
        return false;
    char response[128]{};
    char command[64]{};
    std::snprintf(command, sizeof(command),
                  modem_mqtt::Commands::SET_TOPIC_LENGTH,
                  static_cast<unsigned int>(std::strlen(topic)));
    if (!_modem.sendCommand(command, response, sizeof(response), 2000U)) return false;
    if (!_modem.writeRaw(reinterpret_cast<const uint8_t*>(topic),
                         std::strlen(topic), response, sizeof(response), 2000U)) return false;
    std::snprintf(command, sizeof(command),
                  modem_mqtt::Commands::SET_PAYLOAD_LENGTH,
                  static_cast<unsigned int>(length));
    if (!_modem.sendCommand(command, response, sizeof(response), 2000U)) return false;
    if (!_modem.writeRaw(static_cast<const uint8_t*>(payload), length,
                         response, sizeof(response), 5000U)) return false;
    std::snprintf(command, sizeof(command), modem_mqtt::Commands::PUBLISH,
                  modem_mqtt::Commands::PUBLISH_QOS,
                  modem_mqtt::Commands::PUBLISH_TIMEOUT_SECONDS);
    return _modem.sendCommand(command, response, sizeof(response), 5000U);
}

void CellularMqttClient::disconnect()
{
    char response[96]{};
    char command[64]{};
    if (_connected) {
        std::snprintf(command, sizeof(command), modem_mqtt::Commands::DISCONNECT,
                      modem_mqtt::Commands::PUBLISH_TIMEOUT_SECONDS);
        _modem.sendCommand(command, response, sizeof(response), 5000U);
    }
    _modem.sendCommand(modem_mqtt::Commands::STOP, response, sizeof(response),
                       5000U);
    _connected = false;
}

bool CellularMqttClient::connected() const { return _connected; }

}  // namespace middleware
}  // namespace fortico