#include <unity.h>

#include <cstring>

#include "command_vectors.h"
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

bool decodeHex(const char* hex, uint8_t* output, std::size_t outputLength)
{
    if (hex == nullptr || output == nullptr ||
        std::strlen(hex) != outputLength * 2U) return false;
    for (std::size_t index = 0U; index < outputLength; ++index) {
        const char high = hex[index * 2U];
        const char low = hex[index * 2U + 1U];
        const int highValue = high >= '0' && high <= '9' ? high - '0'
            : high >= 'a' && high <= 'f' ? high - 'a' + 10 : -1;
        const int lowValue = low >= '0' && low <= '9' ? low - '0'
            : low >= 'a' && low <= 'f' ? low - 'a' + 10 : -1;
        if (highValue < 0 || lowValue < 0) return false;
        output[index] = static_cast<uint8_t>((highValue << 4) | lowValue);
    }
    return true;
}

class TestVectorMacVerifier final : public services::ICommandMacVerifier {
public:
    bool verifyCommandMac(const uint8_t* message, std::size_t length,
                          const uint8_t* mac,
                          std::size_t macLength) const override
    {
        return matches(message, length, mac, macLength,
                       command_vectors::validCanonical,
                       command_vectors::validMacHex) ||
               matches(message, length, mac, macLength,
                       command_vectors::expiredCanonical,
                       command_vectors::expiredMacHex);
    }

private:
    static bool matches(const uint8_t* message, std::size_t length,
                        const uint8_t* mac, std::size_t macLength,
                        const char* canonical, const char* macHex)
    {
        uint8_t expected[32]{};
        return message != nullptr && mac != nullptr &&
               std::strlen(canonical) == length &&
               std::memcmp(message, canonical, length) == 0 &&
               macLength == sizeof(expected) &&
               decodeHex(macHex, expected, sizeof(expected)) &&
               std::memcmp(mac, expected, sizeof(expected)) == 0;
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

services::MqttCommandRequest vectorRequest(const char* id, uint64_t sequence,
                                           uint64_t timestamp,
                                           const char* macHex)
{
    services::MqttCommandRequest request{};
    std::strncpy(request.id, id, sizeof(request.id) - 1U);
    std::strncpy(request.command, "STOP", sizeof(request.command) - 1U);
    request.sequence = sequence;
    request.timestamp = timestamp;
    request.hasTimestamp = true;
    decodeHex(macHex, request.mac, sizeof(request.mac));
    return request;
}

void testDocumentedVectorAcceptedThenRejectedAsReplay()
{
    test::MockNvsDriver nvs;
    initializeReplayState(nvs);
    TestVectorMacVerifier verifier;
    const services::MqttCommandRequest request = vectorRequest(
        command_vectors::validId, command_vectors::validSequence,
        command_vectors::validTimestamp, command_vectors::validMacHex);
    char canonical[256]{};
    std::size_t canonicalLength = 0U;
    TEST_ASSERT_TRUE(services::MqttCommandSecurity::buildCanonical(
        request, command_vectors::deviceId, canonical, sizeof(canonical),
        canonicalLength));
    TEST_ASSERT_EQUAL_STRING(command_vectors::validCanonical, canonical);
    const char* reason = nullptr;

    services::MqttCommandSecurity firstBoot(nvs, verifier);
    TEST_ASSERT_TRUE(firstBoot.authorize(
        request, command_vectors::deviceId, true, 1000U, false, reason));
    services::MqttCommandSecurity afterRestart(nvs, verifier);
    TEST_ASSERT_FALSE(afterRestart.authorize(
        request, command_vectors::deviceId, true, 1000U, false, reason));
    TEST_ASSERT_EQUAL_STRING("REPLAY", reason);
}

void testDocumentedExpiredVectorIsRejected()
{
    test::MockNvsDriver nvs;
    initializeReplayState(nvs);
    TestVectorMacVerifier verifier;
    const services::MqttCommandRequest request = vectorRequest(
        command_vectors::expiredId, command_vectors::expiredSequence,
        command_vectors::expiredTimestamp, command_vectors::expiredMacHex);
    char canonical[256]{};
    std::size_t canonicalLength = 0U;
    TEST_ASSERT_TRUE(services::MqttCommandSecurity::buildCanonical(
        request, command_vectors::deviceId, canonical, sizeof(canonical),
        canonicalLength));
    TEST_ASSERT_EQUAL_STRING(command_vectors::expiredCanonical, canonical);
    TEST_ASSERT_TRUE(verifier.verifyCommandMac(
        reinterpret_cast<const uint8_t*>(canonical), canonicalLength,
        request.mac, sizeof(request.mac)));
    const char* reason = nullptr;

    services::MqttCommandSecurity security(nvs, verifier);
    TEST_ASSERT_FALSE(security.authorize(
        request, command_vectors::deviceId, true, 1000U, false, reason));
    TEST_ASSERT_EQUAL_STRING("EXPIRED", reason);
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

void testUnsyncedClockUsesPersistentSequenceWithoutTimeWindow()
{
    test::MockNvsDriver nvs;
    initializeReplayState(nvs);
    TestMacVerifier verifier;
    services::MqttCommandSecurity security(nvs, verifier);
    services::MqttCommandRequest request = stopRequest(1U, "cmd-no-clock");
    request.hasTimestamp = false;
    sign(request);
    const char* reason = nullptr;

    TEST_ASSERT_TRUE(security.authorize(request, "DEVICE-1", false, 0U,
                                        false, reason));
    TEST_ASSERT_NULL(reason);
    TEST_ASSERT_FALSE(security.authorize(request, "DEVICE-1", false, 0U,
                                         false, reason));
    TEST_ASSERT_EQUAL_STRING("REPLAY", reason);
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

    const services::MqttCommandLatch afterRestart;
    TEST_ASSERT_TRUE(afterRestart.engineOnAllowed(true));
}

}  // namespace

void setUp() {}
void tearDown() {}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(testDocumentedVectorAcceptedThenRejectedAsReplay);
    RUN_TEST(testDocumentedExpiredVectorIsRejected);
    RUN_TEST(testValidSignedCommandIsAccepted);
    RUN_TEST(testTamperedCommandIsRejected);
    RUN_TEST(testReplayedCommandIsRejected);
    RUN_TEST(testPreviouslyAcceptedIdIsRejectedWithHigherSequence);
    RUN_TEST(testExpiredCommandIsRejected);
    RUN_TEST(testUnsyncedClockUsesPersistentSequenceWithoutTimeWindow);
    RUN_TEST(testUnenrolledBadgeCannotAuthorizeEngineOn);
    RUN_TEST(testEngineOnRemainsBlockedAfterKillEvenIfAuthStateIsStillActive);
    return UNITY_END();
}
