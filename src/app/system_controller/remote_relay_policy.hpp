#pragma once

#include "types/system_types.h"

namespace fortico {
namespace app {

constexpr types::SystemState stateAfterRemoteCut(types::SystemState current,
                                                 bool killLocked) noexcept
{
	return killLocked || current == types::SystemState::ENGINE_KILL
		? types::SystemState::ENGINE_KILL
		: (current == types::SystemState::SOS_STATE
			   ? types::SystemState::SOS_STATE
			   : types::SystemState::WAIT_BADGE);
}

constexpr bool localAuthenticationMayStartEngine(types::SystemState current,
												 bool killLocked) noexcept
{
	return !killLocked &&
		(current == types::SystemState::WAIT_BADGE ||
		 current == types::SystemState::ALARM_STATE);
}

constexpr bool remoteEngineOnMayBeApplied(types::SystemState current,
										  bool killLocked,
										  bool localAuthenticationActive) noexcept
{
	return !killLocked && localAuthenticationActive &&
		(current == types::SystemState::WAIT_BADGE ||
		 current == types::SystemState::TRACKING_ACTIVE);
}

}  // namespace app
}  // namespace fortico
