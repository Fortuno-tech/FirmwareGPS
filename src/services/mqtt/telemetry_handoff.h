#pragma once

#include <cstddef>

namespace fortico {
namespace services {

class ITelemetryHandoff {
public:
    virtual ~ITelemetryHandoff() = default;
    virtual bool enqueueTelemetry(const char* topic, const void* payload,
                                  std::size_t length) = 0;
};

}  // namespace services
}  // namespace fortico
