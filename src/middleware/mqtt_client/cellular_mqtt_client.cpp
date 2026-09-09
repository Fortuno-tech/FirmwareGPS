#include "middleware/mqtt_client/cellular_mqtt_client.h"

#include <cstdio>
#include <cstring>

#include "config/sim7600_mqtt_config.h"
#include "config/logging_config.h"

namespace {

bool isAtSafeString(const char* value)
{
    return value != nullptr && value[0] != '\0' &&
           std::strchr(value, '"') == nullptr &&
           std::strchr(value, '\r') == nullptr &&
           std::strchr(value, '\n') == nullptr;
}

}  // namespace

namespace fortico {
namespace middleware {

CellularMqttClient::CellularMqttClient(drivers::Sim7600Driver& modem,
                                       core::Logger& logger)
    : _modem(modem), _logger(logger), _credentials{}, _credentialsSet(false),
      _connected(false), _serviceStarted(false), _clientAcquired(false) {}

bool CellularMqttClient::setCredentials(
    const types::MqttCredentials& credentials)
{
    if (!isAtSafeString(credentials.clientId) ||
        !isAtSafeString(credentials.username) ||
        !isAtSafeString(credentials.password)) return false;
    _credentials = credentials;
    _credentialsSet = true;
    return true;
}

bool CellularMqttClient::connect(const char* host, uint16_t port)
{
    if (!_credentialsSet || !isAtSafeString(host) || !_modem.isHealthy()) return false;
    if (std::strlen(host) > 96U || std::strlen(_credentials.clientId) > 63U ||
        std::strlen(_credentials.username) > 63U ||
        std::strlen(_credentials.password) > 95U) return false;

    char response[512]{};
    char command[384]{};
    _logger.info(logging::TAG_MQTT, "verification certificat CA SIM7600");
    if (!_modem.hasCertificate(modem_mqtt::Commands::HIVE_MQ_CA_CERTIFICATE_FILE)) {
        _logger.error(logging::TAG_MQTT,
                      "certificat CA absent du SIM7600: %s",
                      modem_mqtt::Commands::HIVE_MQ_CA_CERTIFICATE_FILE);
        return false;
    }

    _logger.info(logging::TAG_MQTT, "configuration TLS 1.2 et validation CA");
    const int caCommandLength = std::snprintf(
        command, sizeof(command), modem_mqtt::Commands::CONFIGURE_TLS_CA,
        modem_mqtt::Commands::HIVE_MQ_CA_CERTIFICATE_FILE);
    if (caCommandLength < 0 ||
        static_cast<std::size_t>(caCommandLength) >= sizeof(command)) {
        _logger.error(logging::TAG_MQTT, "commande CSSLCFG cacert trop longue");
        return false;
    }
    if (!_modem.sendCommand(modem_mqtt::Commands::CONFIGURE_TLS_VERSION,
                            response, sizeof(response), 3000U)) {
        _logger.error(logging::TAG_MQTT, "CSSLCFG sslversion echoue");
        return false;
    }
    if (!_modem.sendCommand(modem_mqtt::Commands::CONFIGURE_TLS_AUTH_MODE,
                            response, sizeof(response), 3000U)) {
        _logger.error(logging::TAG_MQTT, "CSSLCFG authmode echoue");
        return false;
    }
    if (!_modem.sendCommand(modem_mqtt::Commands::CONFIGURE_TLS_IGNORE_LOCAL_TIME,
                            response, sizeof(response), 3000U)) {
        _logger.error(logging::TAG_MQTT, "CSSLCFG ignorelocaltime echoue");
        return false;
    }
    if (!_modem.sendCommand(modem_mqtt::Commands::CONFIGURE_TLS_SNI,
                            response, sizeof(response), 3000U)) {
        _logger.error(logging::TAG_MQTT, "CSSLCFG enableSNI echoue");
        return false;
    }
    if (!_modem.sendCommand(command, response, sizeof(response), 3000U)) {
        _logger.error(logging::TAG_MQTT, "CSSLCFG cacert echoue");
        return false;
    }

    _logger.info(logging::TAG_MQTT, "demarrage service MQTT");
    if (!_modem.sendMqttCommand(modem_mqtt::Commands::START, "+CMQTTSTART: 0",
                                response, sizeof(response), 15000U)) {
        logMqttFailure("CMQTTSTART", response, "+CMQTTSTART:");
        return false;
    }
    _serviceStarted = true;

    _logger.info(logging::TAG_MQTT, "acquisition client MQTT/TLS");
    const int acquireCommandLength = std::snprintf(
        command, sizeof(command), modem_mqtt::Commands::ACQUIRE_CLIENT,
        _credentials.clientId);
    if (acquireCommandLength < 0 ||
        static_cast<std::size_t>(acquireCommandLength) >= sizeof(command)) {
        _logger.error(logging::TAG_MQTT, "commande CMQTTACCQ trop longue");
        releaseResources();
        return false;
    }
    if (!_modem.sendCommand(command, response, sizeof(response), 5000U)) {
        _logger.error(logging::TAG_MQTT, "CMQTTACCQ echoue");
        releaseResources();
        return false;
    }
    _clientAcquired = true;

    _logger.info(logging::TAG_MQTT, "association contexte TLS MQTT");
    if (!_modem.sendCommand(modem_mqtt::Commands::ASSIGN_TLS_CONTEXT, response,
                            sizeof(response), 5000U)) {
        _logger.error(logging::TAG_MQTT, "CMQTTSSLCFG echoue");
        releaseResources();
        return false;
    }

    _logger.info(logging::TAG_MQTT, "connexion broker MQTT/TLS: %s:%u", host,
                 static_cast<unsigned int>(port));
    const int connectCommandLength = std::snprintf(
        command, sizeof(command), modem_mqtt::Commands::CONNECT, host,
        static_cast<unsigned int>(port), modem_mqtt::Commands::KEEPALIVE_SECONDS,
        _credentials.username, _credentials.password);
    if (connectCommandLength < 0 ||
        static_cast<std::size_t>(connectCommandLength) >= sizeof(command)) {
        _logger.error(logging::TAG_MQTT, "commande CMQTTCONNECT trop longue");
        releaseResources();
        return false;
    }
    _connected = _modem.sendMqttCommand(command, "+CMQTTCONNECT: 0,0", response,
                                        sizeof(response), 60000U);
    if (!_connected) {
        logMqttFailure("CMQTTCONNECT", response, "+CMQTTCONNECT:");
        releaseResources();
        return false;
    }
    _logger.info(logging::TAG_MQTT, "connexion MQTT/TLS etablie");
    return _connected;
}

bool CellularMqttClient::publish(const char* topic, const void* payload,
                                 std::size_t length)
{
    if (!_connected || topic == nullptr || payload == nullptr || length == 0U)
        return false;
    const std::size_t topicLength = std::strlen(topic);
    if (topicLength == 0U || topicLength > 1024U || length > 10240U) return false;
    char response[128]{};
    char command[64]{};
    std::snprintf(command, sizeof(command),
                  modem_mqtt::Commands::SET_TOPIC_LENGTH,
                  static_cast<unsigned int>(topicLength));
    if (!_modem.beginMqttInput(command, response, sizeof(response), 2000U)) return false;
    if (!_modem.writeRaw(reinterpret_cast<const uint8_t*>(topic),
                         topicLength, response, sizeof(response), 2000U)) return false;
    std::snprintf(command, sizeof(command),
                  modem_mqtt::Commands::SET_PAYLOAD_LENGTH,
                  static_cast<unsigned int>(length));
    if (!_modem.beginMqttInput(command, response, sizeof(response), 2000U)) return false;
    if (!_modem.writeRaw(static_cast<const uint8_t*>(payload), length,
                         response, sizeof(response), 5000U)) return false;
    std::snprintf(command, sizeof(command), modem_mqtt::Commands::PUBLISH,
                  modem_mqtt::Commands::PUBLISH_QOS,
                  modem_mqtt::Commands::PUBLISH_TIMEOUT_SECONDS);
    const bool published = _modem.sendMqttCommand(command, "+CMQTTPUB: 0,0",
                                                   response, sizeof(response),
                                                   60000U);
    if (!published) logMqttFailure("CMQTTPUB", response, "+CMQTTPUB:");
    return published;
}

void CellularMqttClient::disconnect()
{
    char response[128]{};
    char command[64]{};
    if (_connected) {
        std::snprintf(command, sizeof(command), modem_mqtt::Commands::DISCONNECT,
                      modem_mqtt::Commands::PUBLISH_TIMEOUT_SECONDS);
        _modem.sendMqttCommand(command, "+CMQTTDISC: 0,0", response,
                               sizeof(response), 60000U);
    }
    releaseResources();
}

bool CellularMqttClient::connected() const { return _connected; }

void CellularMqttClient::logMqttFailure(const char* step, const char* response,
                                        const char* urcPrefix) const
{
    if (response != nullptr && urcPrefix != nullptr) {
        const char* urc = std::strstr(response, urcPrefix);
        if (urc != nullptr) {
            char result[48]{};
            std::size_t length = 0U;
            while (urc[length] != '\0' && urc[length] != '\r' &&
                   urc[length] != '\n' && length + 1U < sizeof(result)) {
                result[length] = urc[length];
                ++length;
            }
            _logger.error(logging::TAG_MQTT, "%s echoue: %s", step, result);
            return;
        }
    }
    _logger.error(logging::TAG_MQTT, "%s echoue: URC final absent ou ERROR AT",
                  step != nullptr ? step : "MQTT");
}

void CellularMqttClient::releaseResources()
{
    char response[128]{};
    if (_clientAcquired) {
        _modem.sendCommand(modem_mqtt::Commands::RELEASE_CLIENT, response,
                           sizeof(response), 5000U);
    }
    if (_serviceStarted) {
        _modem.sendMqttCommand(modem_mqtt::Commands::STOP, "+CMQTTSTOP: 0",
                               response, sizeof(response), 15000U);
    }
    _connected = false;
    _clientAcquired = false;
    _serviceStarted = false;
}

}  // namespace middleware
}  // namespace fortico
