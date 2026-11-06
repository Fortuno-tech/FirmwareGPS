#pragma once

#include <cstdint>

namespace fortico {
namespace app {
namespace network_wait {

constexpr uint32_t kWorkerPollIntervalMs = 250U;
constexpr uint32_t kWorkerStartupTimeoutMs = 1000U;
constexpr uint32_t kWorkerStopTimeoutMs = 5000U;
constexpr uint32_t kProvisioningLockTimeoutMs = 1000U;
constexpr uint32_t kProvisioningCompletionTimeoutMs = 60000U;

inline bool expired(uint32_t startedAtMs, uint32_t nowMs, uint32_t timeoutMs)
{
	return nowMs - startedAtMs >= timeoutMs;
}

inline uint32_t remainingMs(uint32_t startedAtMs, uint32_t nowMs,
							uint32_t timeoutMs)
{
	const uint32_t elapsedMs = nowMs - startedAtMs;
	return elapsedMs >= timeoutMs ? 0U : timeoutMs - elapsedMs;
}

}  // namespace network_wait
}  // namespace app
}  // namespace fortico
