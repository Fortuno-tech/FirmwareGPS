#include <unity.h>

#include "app/system_controller/remote_relay_policy.hpp"

using fortico::app::localAuthenticationMayStartEngine;
using fortico::app::remoteEngineOnMayBeApplied;
using fortico::app::stateAfterRemoteCut;
using fortico::types::SystemState;

void setUp() {}
void tearDown() {}

void testRemoteCutReturnsToBadgeWaitUnlessKillIsLatched()
{
	TEST_ASSERT_EQUAL_INT(
		static_cast<int>(SystemState::WAIT_BADGE),
		static_cast<int>(stateAfterRemoteCut(SystemState::TRACKING_ACTIVE, false)));
	TEST_ASSERT_EQUAL_INT(
		static_cast<int>(SystemState::ENGINE_KILL),
		static_cast<int>(stateAfterRemoteCut(SystemState::ENGINE_KILL, false)));
	TEST_ASSERT_EQUAL_INT(
		static_cast<int>(SystemState::ENGINE_KILL),
		static_cast<int>(stateAfterRemoteCut(SystemState::WAIT_BADGE, true)));
	TEST_ASSERT_EQUAL_INT(
		static_cast<int>(SystemState::SOS_STATE),
		static_cast<int>(stateAfterRemoteCut(SystemState::SOS_STATE, false)));
}

void testKillLatchPreventsLocalAuthenticationFromStartingEngine()
{
	TEST_ASSERT_TRUE(
		localAuthenticationMayStartEngine(SystemState::WAIT_BADGE, false));
	TEST_ASSERT_TRUE(
		localAuthenticationMayStartEngine(SystemState::ALARM_STATE, false));
	TEST_ASSERT_FALSE(
		localAuthenticationMayStartEngine(SystemState::WAIT_BADGE, true));
	TEST_ASSERT_FALSE(
		localAuthenticationMayStartEngine(SystemState::ENGINE_KILL, false));
}

void testRemoteEngineOnRequiresSafeControlStateAndLocalAuthentication()
{
	TEST_ASSERT_TRUE(remoteEngineOnMayBeApplied(
		SystemState::WAIT_BADGE, false, true));
	TEST_ASSERT_TRUE(remoteEngineOnMayBeApplied(
		SystemState::TRACKING_ACTIVE, false, true));
	TEST_ASSERT_FALSE(remoteEngineOnMayBeApplied(
		SystemState::SOS_STATE, false, true));
	TEST_ASSERT_FALSE(remoteEngineOnMayBeApplied(
		SystemState::WAIT_BADGE, true, true));
	TEST_ASSERT_FALSE(remoteEngineOnMayBeApplied(
		SystemState::WAIT_BADGE, false, false));
}

int main()
{
	UNITY_BEGIN();
	RUN_TEST(testRemoteCutReturnsToBadgeWaitUnlessKillIsLatched);
	RUN_TEST(testKillLatchPreventsLocalAuthenticationFromStartingEngine);
	RUN_TEST(testRemoteEngineOnRequiresSafeControlStateAndLocalAuthentication);
	return UNITY_END();
}
