#pragma once

namespace fortico {
namespace services {

class ITemperatureSensor {
public:
    virtual ~ITemperatureSensor() = default;
    virtual bool readCelsius(float& value) = 0;
};

class UnavailableTemperatureSensor final : public ITemperatureSensor {
public:
    bool readCelsius(float& value) override
    {
        value = 0.0F;
        return false;
    }
};

}  // namespace services
}  // namespace fortico
