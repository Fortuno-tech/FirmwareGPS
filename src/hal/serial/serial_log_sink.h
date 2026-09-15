#pragma once

#include "core/logger/logger.h"

namespace fortico {
namespace hal {

/// Adaptateur de sortie console : seul ce HAL connaît Serial/Arduino.
class SerialLogSink final : public core::ILogSink {
public:
    void write(const char* line) override;
};

}  // namespace hal
}  // namespace fortico
