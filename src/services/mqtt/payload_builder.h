#pragma once

#include <cstddef>

#include "types/telemetry_types.h"

namespace fortico {
namespace services {

class PayloadBuilder {
public:
	static bool telemetry(const types::TelemetryPacket& packet,
						  char* output, std::size_t outputSize);
};

}  // namespace services
}  // namespace fortico
