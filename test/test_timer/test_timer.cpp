#include <unity.h>

#include "utils/timer_utils.h"

using namespace fortico;

namespace {
uint32_t g_fakeNow = 0;
uint32_t fakeClock() { return g_fakeNow; }
}

void setUp() {
    g_fakeNow = 0;
    utils::Timer::setClockSourceForTesting(&fakeClock);
}

void tearDown() {
    utils::Timer::resetClockSourceToDefault();
}

void test_millis_uses_injected_clock() {
    g_fakeNow = 1000;
    TEST_ASSERT_EQUAL_UINT32(1000, utils::Timer::millis());
    g_fakeNow = 5000;
    TEST_ASSERT_EQUAL_UINT32(5000, utils::Timer::millis());
}

void test_timeout_before_delay() {
    g_fakeNow = 1500;
    TEST_ASSERT_FALSE(utils::Timer::timeout(1000, 1000));
}

void test_timeout_at_delay() {
    g_fakeNow = 2000;
    TEST_ASSERT_TRUE(utils::Timer::timeout(1000, 1000));
}

void test_timeout_after_delay() {
    g_fakeNow = 3000;
    TEST_ASSERT_TRUE(utils::Timer::timeout(1000, 1000));
}

void test_timeout_handles_uint32_wraparound() {
    const uint32_t startNearMax = 0xFFFFFFF0;
    g_fakeNow = 5;
    TEST_ASSERT_TRUE(utils::Timer::timeout(startNearMax, 21));
    TEST_ASSERT_FALSE(utils::Timer::timeout(startNearMax, 22));
    TEST_ASSERT_TRUE(utils::Timer::timeout(startNearMax, 20));
}

void test_reset_clock_source_restores_default() {
    utils::Timer::resetClockSourceToDefault();
    (void)utils::Timer::millis();
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_millis_uses_injected_clock);
    RUN_TEST(test_timeout_before_delay);
    RUN_TEST(test_timeout_at_delay);
    RUN_TEST(test_timeout_after_delay);
    RUN_TEST(test_timeout_handles_uint32_wraparound);
    RUN_TEST(test_reset_clock_source_restores_default);
    return UNITY_END();
}
