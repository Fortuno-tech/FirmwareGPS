#include "mqtt_service.h"

MQTTService::MQTTService(
    Client& client
)
    : _client(client),
      _broker(""),
      _port(1883),
      _clientId("")
{
}

void MQTTService::begin(
    const char* broker,
    uint16_t port,
    const char* clientId
)
{
    _broker = broker;
    _port = port;
    _clientId = clientId != nullptr ? String(clientId) : "fortico-device";

    _client.setServer(
        _broker.c_str(),
        _port
    );
}

bool MQTTService::connect(
    const String& deviceId,
    const char* username,
    const char* password
)
{
    if (_client.connected())
    {
        return true;
    }

    String clientId = _clientId.length() > 0 ? _clientId : deviceId;

    bool connected = false;

    if (username != nullptr && password != nullptr)
    {
        connected = _client.connect(
            clientId.c_str(),
            username,
            password
        );
    }
    else
    {
        connected = _client.connect(
            clientId.c_str()
        );
    }

    return connected;
}

bool MQTTService::publish(
    const String& topic,
    const String& payload,
    bool retained
)
{
    if (!_client.connected())
    {
        return false;
    }

    return _client.publish(
        topic.c_str(),
        payload.c_str(),
        retained
    );
}

bool MQTTService::publishFormatted(
    const char* topicTemplate,
    const String& vehicleId,
    const String& payload
)
{
    String topic = buildTopic(
        topicTemplate,
        vehicleId
    );

    return publish(
        topic,
        payload
    );
}

bool MQTTService::subscribe(
    const String& topic
)
{
    if (!_client.connected())
    {
        return false;
    }

    return _client.subscribe(
        topic.c_str()
    );
}

bool MQTTService::loop()
{
    if (!_client.connected())
    {
        return false;
    }

    _client.loop();

    return true;
}

bool MQTTService::isConnected()
{
    return _client.connected();
}

String MQTTService::buildTopic(
    const char* topicTemplate,
    const String& vehicleId
) const
{
    char buffer[128];

    snprintf(
        buffer,
        sizeof(buffer),
        topicTemplate,
        vehicleId.c_str()
    );

    return String(buffer);
}
