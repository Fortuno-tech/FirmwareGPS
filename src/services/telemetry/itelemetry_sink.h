#pragma once

#include <cstddef>

namespace fortico {
namespace services {

class ITelemetrySink {
public:
    virtual ~ITelemetrySink() = default;
    virtual bool begin() { return true; }
    virtual bool publish(const char* payload, std::size_t length,
                         bool highPriority = false) = 0;
    virtual bool publishChannel(const char* channel, const char* payload,
                                std::size_t length,
                                bool highPriority = false)
    {
        (void)channel;
        (void)payload;
        (void)length;
        (void)highPriority;
        return false;
    }
};

}  // namespace services
}  // namespace fortico
