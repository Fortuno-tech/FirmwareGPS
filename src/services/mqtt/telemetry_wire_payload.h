#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

namespace fortico {
namespace services {

class TelemetryWirePayload final {
public:
    static bool withOutboxSequence(const uint8_t* payload, std::size_t length,
                                   uint64_t sequence, std::string& output)
    {
        if (payload == nullptr || length < 2U || payload[0] != '{' ||
            payload[length - 1U] != '}') {
            return false;
        }
        char sequenceField[40]{};
        const int fieldLength = std::snprintf(
            sequenceField, sizeof(sequenceField), "\"outboxSeq\":%llu",
            static_cast<unsigned long long>(sequence));
        if (fieldLength <= 0 ||
            static_cast<std::size_t>(fieldLength) >= sizeof(sequenceField)) {
            return false;
        }

        output.clear();
        output.reserve(length + static_cast<std::size_t>(fieldLength));
        output.append(reinterpret_cast<const char*>(payload), 1U);
        output.append(sequenceField, static_cast<std::size_t>(fieldLength));
        if (length > 2U) {
            output.push_back(',');
        }
        output.append(reinterpret_cast<const char*>(payload + 1U), length - 1U);
        return true;
    }
};

}  // namespace services
}  // namespace fortico
