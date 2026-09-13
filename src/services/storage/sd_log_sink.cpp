#include "services/storage/sd_log_sink.h"

#include <cstring>

namespace fortico {
namespace services {

void SdLogSink::write(const char* line)
{
    if (line == nullptr) return;
    // SdDriver::append opens and closes on every operation, so the data is
    // flushed even if the device loses power immediately afterwards.
    static constexpr const char* kLogPath = "/fortico.log";
    const std::size_t length = std::strlen(line);
    if (length == 0U) return;
    (void)_driver.append(kLogPath, line, length);
    static constexpr char newline = '\n';
    (void)_driver.append(kLogPath, &newline, 1U);
}

}  // namespace services
}  // namespace fortico
