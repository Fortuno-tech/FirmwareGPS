#ifndef MQTT_SERVICE_H
#define MQTT_SERVICE_H

#include <Arduino.h>
#include <Client.h>
#include <PubSubClient.h>

#include "mqtt_topics.h"

class MQTTService
{
public:

    explicit MQTTService(
        Client& client
    );

    void begin(
        const char* broker,
        uint16_t port,
        const char* clientId = nullptr
    );

    bool connect(
        const String& deviceId,
        const char* username = nullptr,
        const char* password = nullptr
    );

    bool publish(
        const String& topic,
        const String& payload,
        bool retained = false
    );

    bool publishFormatted(
        const char* topicTemplate,
        const String& vehicleId,
        const String& payload
    );

    bool subscribe(
        const String& topic
    );

    bool loop();

    bool isConnected();

    String buildTopic(
        const char* topicTemplate,
        const String& vehicleId
    ) const;

private:

    PubSubClient _client;

    String _broker;
    uint16_t _port;
    String _clientId;
};

#endif
