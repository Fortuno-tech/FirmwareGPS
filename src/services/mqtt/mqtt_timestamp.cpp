#include "services/mqtt/mqtt_timestamp.h"

#include <cstdio>

namespace fortico {
namespace services {

bool MqttTimestamp::format(const drivers::RtcDateTime& dateTime,
                           char* output, std::size_t outputSize)
{
    if (output == nullptr || outputSize < 21U ||
        dateTime.year < 2000U || dateTime.year > 2099U ||
        dateTime.month < 1U || dateTime.month > 12U || dateTime.day < 1U ||
        dateTime.day > 31U || dateTime.hour > 23U || dateTime.minute > 59U ||
        dateTime.second > 59U) {
        return false;
    }
    const int written = std::snprintf(
        output, outputSize, "%04u-%02u-%02uT%02u:%02u:%02uZ",
        static_cast<unsigned int>(dateTime.year),
        static_cast<unsigned int>(dateTime.month),
        static_cast<unsigned int>(dateTime.day),
        static_cast<unsigned int>(dateTime.hour),
        static_cast<unsigned int>(dateTime.minute),
        static_cast<unsigned int>(dateTime.second));
    return written == 20;
}

}  // namespace services
}  // namespace fortico
