#include <unity.h>

#include "app/connectivity_task/network_wait_policy.hpp"
#include "app/control_timing.hpp"

using fortico::app::ControlTimingMetrics;
using fortico::app::network_wait::expired;
using fortico::app::network_wait::remainingMs;

void setUp() {}
void tearDown() {}

void testControlCycleMaximumUsesUnsignedWrapSafeDuration()
{
	ControlTimingMetrics metrics;
	metrics.recordControlCycle(0xFFFFFFF0U, 0x00000010U);
	metrics.recordControlCycle(100U, 125U);
	TEST_ASSERT_EQUAL_UINT32(32U, metrics.maxControlCycleMs());
}

void testCommandToRelayLatencyTracksMaximumAndCount()
{
	ControlTimingMetrics metrics;
	metrics.recordCommandRelayAction(100U, 145U);
	metrics.recordCommandRelayAction(0xFFFFFFF0U, 0x00000020U);
	TEST_ASSERT_EQUAL_UINT32(48U, metrics.maxCommandRelayLatencyMs());
	TEST_ASSERT_EQUAL_UINT32(2U, metrics.commandRelayActionCount());
}

void testNetworkTimeoutUsesExactDeadlineAndWrapSafeElapsedTime()
{
	TEST_ASSERT_FALSE(expired(100U, 199U, 100U));
	TEST_ASSERT_TRUE(expired(100U, 200U, 100U));
	TEST_ASSERT_FALSE(expired(0xFFFFFFF0U, 0x00000010U, 33U));
	TEST_ASSERT_TRUE(expired(0xFFFFFFF0U, 0x00000011U, 33U));
}

void testRemainingWaitClampsAtZeroAndHandlesTickWrap()
{
	TEST_ASSERT_EQUAL_UINT32(50U, remainingMs(100U, 150U, 100U));
	TEST_ASSERT_EQUAL_UINT32(0U, remainingMs(100U, 201U, 100U));
	TEST_ASSERT_EQUAL_UINT32(32U,
		remainingMs(0xFFFFFFF0U, 0x00000010U, 64U));
}

int main()
{
	UNITY_BEGIN();
	RUN_TEST(testControlCycleMaximumUsesUnsignedWrapSafeDuration);
	RUN_TEST(testCommandToRelayLatencyTracksMaximumAndCount);
	RUN_TEST(testNetworkTimeoutUsesExactDeadlineAndWrapSafeElapsedTime);
	RUN_TEST(testRemainingWaitClampsAtZeroAndHandlesTickWrap);
	return UNITY_END();
}
