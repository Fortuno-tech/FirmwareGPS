#pragma once

namespace fortico {
namespace interfaces {

class IFuelDriver {
public:
    virtual ~IFuelDriver() = default;

    virtual bool readVoltage(float& voltage) = 0;

    virtual bool readRaw(float& raw) = 0;
};

}  // namespace interfaces
}  // namespace fortico