#pragma once

#include "middleware/mqtt_client/mqtt_client.h"
#include "middleware/tls/tls_client.h"

#ifdef ESP32_TARGET
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#endif

namespace fortico {
namespace middleware {

class EspMqttClient final : public MqttClient {
public:
    explicit EspMqttClient(TlsClient& tls);
    bool connect(const char* host, uint16_t port) override;
    bool publish(const char* topic, const void* payload,
                 std::size_t length) override;
    void disconnect() override;
    bool connected() const;
    void loop();

private:
    TlsClient& _tls;
#ifdef ESP32_TARGET
    WiFiClientSecure _secureClient;
    PubSubClient _client;
#endif
    bool _connected;
};

}  // namespace middleware
}  // namespace fortico