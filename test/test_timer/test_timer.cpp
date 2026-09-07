/* #include <unity.h>
#include <cstdint>

#include "utils/timer_utils.h"
#include "../src/utils/timer_utils.cpp"
using namespace fortico;

namespace {

uint32_t g_fakeNow = 0;

uint32_t fakeClock()
{
    return g_fakeNow;
}

}

void setUp()
{
    g_fakeNow = 0;
    utils::Timer::setClockSourceForTesting(&fakeClock);
}

void tearDown()
{
    utils::Timer::resetClockSourceToDefault();
}


// --------------------------------------------------
// Test 1
// --------------------------------------------------

void test_millis_reflete_la_source_injectee()
{
    g_fakeNow = 1000;
    TEST_ASSERT_EQUAL_UINT32(1000, utils::Timer::millis());

    g_fakeNow = 5000;
    TEST_ASSERT_EQUAL_UINT32(5000, utils::Timer::millis());
}


// --------------------------------------------------
// Test 2
// --------------------------------------------------

void test_timeout_avant_echeance()
{
    uint32_t start = 1000;

    g_fakeNow = 1500;

    TEST_ASSERT_FALSE(
        utils::Timer::timeout(start, 1000)
    );
}


// --------------------------------------------------
// Test 3
// --------------------------------------------------

void test_timeout_exactement_a_lecheance()
{
    uint32_t start = 1000;

    g_fakeNow = 2000;

    TEST_ASSERT_TRUE(
        utils::Timer::timeout(start, 1000)
    );
}


// --------------------------------------------------
// Test 4
// --------------------------------------------------

void test_timeout_apres_echeance()
{
    uint32_t start = 1000;

    g_fakeNow = 3000;

    TEST_ASSERT_TRUE(
        utils::Timer::timeout(start, 1000)
    );
}


// --------------------------------------------------
// Test 5 : wraparound uint32_t
// --------------------------------------------------

void test_timeout_wraparound_32_bits()
{
    const uint32_t startNearMax = 0xFFFFFFF0;

    g_fakeNow = 5;

    // 21 ms réellement écoulées
    TEST_ASSERT_TRUE(
        utils::Timer::timeout(startNearMax, 21)
    );

    TEST_ASSERT_FALSE(
        utils::Timer::timeout(startNearMax, 22)
    );

    TEST_ASSERT_TRUE(
        utils::Timer::timeout(startNearMax, 20)
    );
}


// --------------------------------------------------
// Test 6
// --------------------------------------------------

void test_reset_clock_source()
{
    utils::Timer::resetClockSourceToDefault();

    uint32_t realNow = utils::Timer::millis();

    (void)realNow;

    // Le simple retour sans crash suffit ici.
    TEST_PASS();
}


// --------------------------------------------------
// Test 7
// --------------------------------------------------

void test_micros_ne_plante_pas()
{
    uint64_t nowUs = utils::Timer::micros();

    (void)nowUs;

    TEST_PASS();
}


// --------------------------------------------------
// Point d'entrée Unity
// --------------------------------------------------

int main()
{
    UNITY_BEGIN();

    RUN_TEST(test_millis_reflete_la_source_injectee);
    RUN_TEST(test_timeout_avant_echeance);
    RUN_TEST(test_timeout_exactement_a_lecheance);
    RUN_TEST(test_timeout_apres_echeance);
    RUN_TEST(test_timeout_wraparound_32_bits);
    RUN_TEST(test_reset_clock_source);
    RUN_TEST(test_micros_ne_plante_pas);

    return UNITY_END();
}
 */