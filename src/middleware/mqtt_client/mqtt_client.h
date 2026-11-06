#pragma once
#include <cstddef>
#include <cstdint>
namespace fortico { namespace middleware {
class MqttClient {
public:
    virtual ~MqttClient() = default;
    virtual bool connect(const char* host, uint16_t port) = 0;
    virtual bool publish(const char* topic, const void* payload, std::size_t length) = 0;
    virtual void disconnect() = 0;
};
} }