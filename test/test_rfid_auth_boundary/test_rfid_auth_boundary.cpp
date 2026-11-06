#include <unity.h>

#include <cstring>

#include "config/security_config.h"
#include "managers/authentication_manager/authentication_manager.hpp"
#include "mocks/mock_nvs_driver.h"
#include "services/security/badge_hasher.h"
#include "services/storage/storage_service.h"
#include "services/storage/storage_service.cpp"
#include "managers/authentication_manager/authentication_manager.cpp"

using namespace fortico;

namespace {

class TestBadgeHasher final : public services::IBadgeHasher {
public:
    bool isRunning() const override { return true; }

    bool hashBadge(const char* uid, uint8_t* digest,
                   std::size_t digestSize) const override
    {
        if (uid == nullptr || uid[0] == '\0' || digest == nullptr ||
            digestSize < DigestSize) return false;
        uint32_t hash = 2166136261U;
        for (const char* cursor = uid; *cursor != '\0'; ++cursor) {
            hash = (hash ^ static_cast<uint8_t>(*cursor)) * 16777619U;
        }
        for (std::size_t index = 0U; index < DigestSize; ++index) {
            digest[index] = static_cast<uint8_t>(
                hash >> ((index % sizeof(hash)) * 8U));
        }
        return true;
    }
};

struct PersistedBadges {
    uint32_t count;
    uint8_t hashes[services::StorageService::MaxBadges]
                  [services::IBadgeHasher::DigestSize];
};

void seedLegacySharedBadge(test::MockNvsDriver& nvs,
                           const TestBadgeHasher& hasher)
{
    PersistedBadges badges{};
    badges.count = 1U;
    TEST_ASSERT_TRUE(hasher.hashBadge(
        "91:51:E2:06", badges.hashes[0], sizeof(badges.hashes[0])));
    TEST_ASSERT_TRUE(nvs.open(security::NVS_NAMESPACE, false));
    TEST_ASSERT_TRUE(nvs.putBlob(security::NVS_KEY_RFID_WHITELIST,
                                 &badges, sizeof(badges)));
    nvs.close();
}

void testUnenrolledAndLegacySharedBadgesAreDeniedAtAuthenticationBoundary()
{
    test::MockNvsDriver nvs;
    TestBadgeHasher hasher;
    seedLegacySharedBadge(nvs, hasher);

    services::StorageService storage(nvs, hasher);
    TEST_ASSERT_TRUE(storage.start());
    TEST_ASSERT_FALSE(storage.isBadgeAuthorized("91:51:E2:06"));

    managers::AuthenticationManager authentication;
    authentication.setCredentialStore(&storage);
    authentication.init();
    TEST_ASSERT_FALSE(authentication.authenticate("91:51:E2:06", 1U));
    TEST_ASSERT_EQUAL(types::AuthState::UNAUTHENTICATED,
                      authentication.state());
    TEST_ASSERT_EQUAL(types::AuthenticationResult::UNKNOWN_CARD,
                      authentication.decision().result);
    TEST_ASSERT_FALSE(authentication.authenticate("12:34:56:78", 2U));
    TEST_ASSERT_EQUAL(types::AuthState::UNAUTHENTICATED,
                      authentication.state());
}

void testIndividuallyAddedBadgeIsAcceptedAndRemovalDeniesIt()
{
    test::MockNvsDriver nvs;
    TestBadgeHasher hasher;
    services::StorageService storage(nvs, hasher);
    TEST_ASSERT_TRUE(storage.start());

    managers::AuthenticationManager authentication;
    authentication.setCredentialStore(&storage);
    authentication.init();

    TEST_ASSERT_TRUE(storage.addBadge("12:34:56:78"));
    TEST_ASSERT_TRUE(authentication.authenticate("12:34:56:78", 3U));
    TEST_ASSERT_EQUAL(types::AuthState::AUTHENTICATED,
                      authentication.state());

    TEST_ASSERT_TRUE(storage.removeBadge("12:34:56:78"));
    TEST_ASSERT_FALSE(authentication.authenticate("12:34:56:78", 4U));
    TEST_ASSERT_EQUAL(types::AuthState::UNAUTHENTICATED,
                      authentication.state());
}

}  // namespace

void setUp() {}
void tearDown() {}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(testUnenrolledAndLegacySharedBadgesAreDeniedAtAuthenticationBoundary);
    RUN_TEST(testIndividuallyAddedBadgeIsAcceptedAndRemovalDeniesIt);
    return UNITY_END();
}
