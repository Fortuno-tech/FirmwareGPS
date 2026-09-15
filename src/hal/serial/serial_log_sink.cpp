#include "hal/serial/serial_log_sink.h"

#ifdef ESP32_TARGET
#include <Arduino.h>
#endif

namespace fortico {
namespace hal {

void SerialLogSink::write(const char* line)
{
#ifdef ESP32_TARGET
    if (line != nullptr) Serial.println(line);
#else
    (void)line;
#endif
}

}  // namespace hal
}  // namespace fortico
