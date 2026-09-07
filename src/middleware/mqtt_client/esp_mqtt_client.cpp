#include "middleware/mqtt_client/esp_mqtt_client.h"

namespace fortico {
namespace middleware {

EspMqttClient::EspMqttClient(TlsClient& tls)
    : _tls(tls)
#ifdef ESP32_TARGET
      , _secureClient(), _client(_secureClient)
#endif
      , _connected(false)
{
}

bool EspMqttClient::connect(const char* host, uint16_t port)
{
    if (!_tls.isConfigured() || host == nullptr) return false;
#ifdef ESP32_TARGET
    if (_tls.certificates() == nullptr) return false;
    _secureClient.setCACert(_tls.certificates()->ca());
    _client.setServer(host, port);
    _connected = _client.connect("fortico-device");
    return _connected;
#else
    (void)port;
    return false;
#endif
}

bool EspMqttClient::publish(const char* topic, const void* payload,
                            std::size_t length)
{
    if (!_connected || topic == nullptr || payload == nullptr) return false;
#ifdef ESP32_TARGET
    return _client.publish(topic,
                           static_cast<const uint8_t*>(payload), length);
#else
    (void)length;
    return false;
#endif
}

void EspMqttClient::disconnect()
{
#ifdef ESP32_TARGET
    _client.disconnect();
#endif
    _connected = false;
}

bool EspMqttClient::connected() const { return _connected; }

void EspMqttClient::loop()
{
#ifdef ESP32_TARGET
    if (_connected) _client.loop();
#endif
}

}  // namespace middleware
}  // namespace fortico