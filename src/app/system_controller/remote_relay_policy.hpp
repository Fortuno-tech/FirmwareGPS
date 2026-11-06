#pragma once

#include <cmath>

#include "config/safety_config.h"
#include "types/gps_types.h"
#include "types/system_types.h"

namespace fortico {
namespace app {

inline bool remoteKillMayBeApplied(const types::GpsData& gps,
                                   uint32_t nowMs, float maxSpeedKmh,
                                   uint32_t maxFixAgeMs) noexcept
{
	if (!gps.fixValid || !gps.hasLastFix || !std::isfinite(gps.speedKmh) ||
		gps.speedKmh < 0.0F || gps.speedKmh > maxSpeedKmh) {
		return false;
	}
	const uint32_t fixAgeMs =
		nowMs - static_cast<uint32_t>(gps.timestamp);
	return fixAgeMs <= maxFixAgeMs;
}

inline bool remoteKillMayBeApplied(const types::GpsData& gps,
								   uint32_t nowMs) noexcept
{
	return remoteKillMayBeApplied(gps, nowMs, safety::KILL_MAX_SPEED_KMH,
								  safety::KILL_GPS_MAX_AGE_MS);
}

constexpr types::SystemState stateAfterRemoteCut(types::SystemState current,
                                                 bool killLocked) noexcept
{
	return safety::KILL_LOCK_UNTIL_REBOOT &&
			(killLocked || current == types::SystemState::ENGINE_KILL)
		? types::SystemState::ENGINE_KILL
		: (current == types::SystemState::SOS_STATE
			   ? types::SystemState::SOS_STATE
			   : types::SystemState::WAIT_BADGE);
}

constexpr bool localAuthenticationMayStartEngine(types::SystemState current,
												 bool killLocked) noexcept
{
	return (!safety::KILL_LOCK_UNTIL_REBOOT || !killLocked) &&
		(current == types::SystemState::WAIT_BADGE ||
		 current == types::SystemState::ALARM_STATE);
}

constexpr bool remoteEngineOnMayBeApplied(types::SystemState current,
										  bool killLocked,
										  bool localAuthenticationActive) noexcept
{
	return (!safety::KILL_LOCK_UNTIL_REBOOT || !killLocked) &&
		(!safety::ENGINE_ON_REQUIRE_LOCAL_AUTH || localAuthenticationActive) &&
		(!safety::ENGINE_ON_REQUIRE_SAFE_STATE ||
		 (current == types::SystemState::WAIT_BADGE ||
		  current == types::SystemState::TRACKING_ACTIVE));
}

}  // namespace app
}  // namespace fortico
