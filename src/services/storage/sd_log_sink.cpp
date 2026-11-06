#include "services/storage/sd_log_sink.h"

#include <cstring>

namespace fortico {
namespace services {

void SdLogSink::write(const char* line)
{
    if (line == nullptr) return;
    // SdDriver::append opens and closes on every operation, so the data is
    // flushed even if the device loses power immediately afterwards.
    const std::size_t length = std::strlen(line);
    if (length == 0U) return;
    if (!storage::ensureLogDirectories(_driver)) {
        return;
    }
    (void)_driver.append(storage::kTextLogPath, line, length);
    static constexpr char newline = '\n';
    (void)_driver.append(storage::kTextLogPath, &newline, 1U);
}

}  // namespace services
}  // namespace fortico
