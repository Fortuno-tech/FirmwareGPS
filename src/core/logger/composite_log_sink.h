#pragma once

#include "core/logger/logger.h"

namespace fortico {
namespace core {

/// Duplique chaque ligne de diagnostic vers deux destinations indépendantes.
class CompositeLogSink final : public ILogSink {
public:
    CompositeLogSink(ILogSink& first, ILogSink& second)
        : _first(first), _second(second) {}

    void write(const char* line) override {
        _first.write(line);
        _second.write(line);
    }

private:
    ILogSink& _first;
    ILogSink& _second;
};

}  // namespace core
}  // namespace fortico
