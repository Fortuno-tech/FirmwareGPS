#include <unity.h>

#include <cstring>

#include "mocks/mock_nvs_driver.h"
#include "services/mqtt/mqtt_command_security.h"
#include "services/mqtt/mqtt_command_security.cpp"

using namespace fortico;

namespace {

class TestMacVerifier final : public services::ICommandMacVerifier {
public:
    bool verifyCommandMac(const uint8_t* message, std::size_t length,
                          const uint8_t* mac,
                          std::size_t macLength) const override
    {
        uint8_t expected[32]{};
        makeMac(message, length, expected);
        if (macLength != sizeof(expected)) return false;
        uint8_t difference = 0U;
        for (std::size_t index = 0U; index < sizeof(expected); ++index) {
            difference |= static_cast<uint8_t>(expected[index] ^ mac[index]);
        }
        return difference == 0U;
    }

    static void makeMac(const uint8_t* message, std::size_t length,
                        uint8_t* mac)
    {
        uint32_t hash = 2166136261U;
        for (std::size_t index = 0U; index < length; ++index) {
            hash = (hash ^ message[index]) * 16777619U;
        }
        for (std::size_t index = 0U; index < 32U; ++index) {
            mac[index] = static_cast<uint8_t>(hash >> ((index % 4U) * 8U));
        }
    }
};

void initializeReplayState(test::MockNvsDriver& nvs)
{
    types::MqttCommandReplayState state{};
    TEST_ASSERT_TRUE(nvs.open(security::NVS_NAMESPACE, false));
    TEST_ASSERT_TRUE(nvs.putBlob(security::NVS_KEY_MQTT_COMMAND_STATE,
                                 &state, sizeof(state)));
    nvs.close();
}

void sign(services::MqttCommandRequest& request)
{
    char canonical[256]{};
    std::size_t length = 0U;
    TEST_ASSERT_TRUE(services::MqttCommandSecurity::buildCanonical(
        request, "DEVICE-1", canonical, sizeof(canonical), length));
    TestMacVerifier::makeMac(
        reinterpret_cast<const uint8_t*>(canonical), length, request.mac);
}

services::MqttCommandRequest stopRequest(uint64_t sequence, const char* id)
{
    services::MqttCommandRequest request{};
    std::strncpy(request.id, id, sizeof(request.id) - 1U);
    std::strncpy(request.command, "STOP", sizeof(request.command) - 1U);
    request.sequence = sequence;
    request.timestamp = 1000U;
    request.hasTimestamp = true;
    sign(request);
    return request;
}

void testValidSignedCommandIsAccepted()
{
    test::MockNvsDriver nvs;
    initializeReplayState(nvs);
    TestMacVerifier verifier;
    services::MqttCommandSecurity security(nvs, verifier);
    const services::MqttCommandRequest request = stopRequest(1U, "cmd-1");
    const char* reason = nullptr;

    TEST_ASSERT_TRUE(security.authorize(request, "DEVICE-1", true, 1000U,
                                        false, reason));
    TEST_ASSERT_NULL(reason);
}

void testTamperedCommandIsRejected()
{
    test::MockNvsDriver nvs;
    initializeReplayState(nvs);
    TestMacVerifier verifier;
    services::MqttCommandSecurity security(nvs, verifier);
    services::MqttCommandRequest request = stopRequest(1U, "cmd-1");
    std::strncpy(request.command, "ENGINE_OFF", sizeof(request.command) - 1U);
    const char* reason = nullptr;

    TEST_ASSERT_FALSE(security.authorize(request, "DEVICE-1", true, 1000U,
                                         false, reason));
    TEST_ASSERT_EQUAL_STRING("BAD_MAC", reason);
}

void testReplayedCommandIsRejected()
{
    test::MockNvsDriver nvs;
    initializeReplayState(nvs);
    TestMacVerifier verifier;
    const services::MqttCommandRequest request = stopRequest(1U, "cmd-1");
    const char* reason = nullptr;

    services::MqttCommandSecurity firstBoot(nvs, verifier);
    TEST_ASSERT_TRUE(firstBoot.authorize(request, "DEVICE-1", true, 1000U,
                                        false, reason));
    services::MqttCommandSecurity afterRestart(nvs, verifier);
    TEST_ASSERT_FALSE(afterRestart.authorize(request, "DEVICE-1", true, 1000U,
                                         false, reason));
    TEST_ASSERT_EQUAL_STRING("REPLAY", reason);
}

void testPreviouslyAcceptedIdIsRejectedWithHigherSequence()
{
    test::MockNvsDriver nvs;
    initializeReplayState(nvs);
    TestMacVerifier verifier;
    const services::MqttCommandRequest first = stopRequest(1U, "cmd-1");
    const services::MqttCommandRequest duplicate = stopRequest(2U, "cmd-1");
    const char* reason = nullptr;

    services::MqttCommandSecurity firstBoot(nvs, verifier);
    TEST_ASSERT_TRUE(firstBoot.authorize(first, "DEVICE-1", true, 1000U,
                                        false, reason));
    services::MqttCommandSecurity afterRestart(nvs, verifier);
    TEST_ASSERT_FALSE(afterRestart.authorize(duplicate, "DEVICE-1", true, 1000U,
                                         false, reason));
    TEST_ASSERT_EQUAL_STRING("REPLAY", reason);
}

void testExpiredCommandIsRejected()
{
    test::MockNvsDriver nvs;
    initializeReplayState(nvs);
    TestMacVerifier verifier;
    services::MqttCommandSecurity security(nvs, verifier);
    services::MqttCommandRequest request = stopRequest(1U, "cmd-1");
    request.timestamp = 900U;
    sign(request);
    const char* reason = nullptr;

    TEST_ASSERT_FALSE(security.authorize(request, "DEVICE-1", true, 1000U,
                                         false, reason));
    TEST_ASSERT_EQUAL_STRING("EXPIRED", reason);
}

void testUnenrolledBadgeCannotAuthorizeEngineOn()
{
    test::MockNvsDriver nvs;
    initializeReplayState(nvs);
    TestMacVerifier verifier;
    services::MqttCommandSecurity security(nvs, verifier);
    services::MqttCommandRequest request{};
    std::strncpy(request.id, "cmd-1", sizeof(request.id) - 1U);
    std::strncpy(request.command, "ENGINE_ON", sizeof(request.command) - 1U);
    request.sequence = 1U;
    request.timestamp = 1000U;
    request.hasTimestamp = true;
    sign(request);
    const char* reason = nullptr;

    TEST_ASSERT_FALSE(security.authorize(request, "DEVICE-1", true, 1000U,
                                         false, reason));
    TEST_ASSERT_EQUAL_STRING("LOCAL_AUTH_REQUIRED", reason);
}

void testEngineOnRemainsBlockedAfterKillEvenIfAuthStateIsStillActive()
{
    services::MqttCommandLatch latch;
    TEST_ASSERT_TRUE(latch.engineOnAllowed(true));
    latch.latchAfterKill();
    TEST_ASSERT_FALSE(latch.engineOnAllowed(true));
    TEST_ASSERT_FALSE(latch.engineOnAllowed(false));
}

}  // namespace

void setUp() {}
void tearDown() {}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(testValidSignedCommandIsAccepted);
    RUN_TEST(testTamperedCommandIsRejected);
    RUN_TEST(testReplayedCommandIsRejected);
    RUN_TEST(testPreviouslyAcceptedIdIsRejectedWithHigherSequence);
    RUN_TEST(testExpiredCommandIsRejected);
    RUN_TEST(testUnenrolledBadgeCannotAuthorizeEngineOn);
    RUN_TEST(testEngineOnRemainsBlockedAfterKillEvenIfAuthStateIsStillActive);
    return UNITY_END();
}
