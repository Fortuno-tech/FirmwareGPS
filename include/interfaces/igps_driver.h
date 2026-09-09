#pragma once

#include "interfaces/idriver.h"
#include "types/gps_types.h"

namespace fortico {
namespace interfaces {

class IGpsDriver : public IDriver {
public:
    virtual bool enableGnss() = 0;
    virtual bool disableGnss() = 0;
    virtual bool readPosition(types::GpsData& position) = 0;
};

}  // namespace interfaces
}  // namespace fortico
