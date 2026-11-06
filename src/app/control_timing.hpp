#pragma once

#include <cstdint>

namespace fortico {
namespace app {

class ControlTimingMetrics final {
public:
	void recordControlCycle(uint32_t startedAtMs, uint32_t completedAtMs)
	{
		const uint32_t durationMs = completedAtMs - startedAtMs;
		if (durationMs > _maxControlCycleMs) {
			_maxControlCycleMs = durationMs;
		}
	}

	void recordCommandRelayAction(uint32_t receivedAtMs, uint32_t actionAtMs)
	{
		const uint32_t latencyMs = actionAtMs - receivedAtMs;
		if (latencyMs > _maxCommandRelayLatencyMs) {
			_maxCommandRelayLatencyMs = latencyMs;
		}
		++_commandRelayActionCount;
	}

	uint32_t maxControlCycleMs() const { return _maxControlCycleMs; }
	uint32_t maxCommandRelayLatencyMs() const
	{
		return _maxCommandRelayLatencyMs;
	}
	uint32_t commandRelayActionCount() const { return _commandRelayActionCount; }

private:
	uint32_t _maxControlCycleMs = 0U;
	uint32_t _maxCommandRelayLatencyMs = 0U;
	uint32_t _commandRelayActionCount = 0U;
};

}  // namespace app
}  // namespace fortico
