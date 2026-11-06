#include <unity.h>

#include <cstring>

#include "config/security_config.h"
#include "mocks/mock_nvs_driver.h"
#include "services/security/badge_hasher.h"
#include "services/storage/storage_service.h"
#include "services/storage/storage_service.cpp"

using namespace fortico;

namespace {

struct TestVersionedBadgeWhitelist {
    uint32_t magic;
    uint16_t version;
    uint16_t count;
    uint8_t badges[services::StorageService::MaxBadges]
                  [services::IBadgeHasher::DigestSize];
};

class TestBadgeHasher final : public services::IBadgeHasher {
public:
    bool isRunning() const override { return true; }

    bool hashBadge(const char* uid, uint8_t* digest,
                   std::size_t digestSize) const override
    {
        if (uid == nullptr || digest == nullptr ||
            digestSize != DigestSize) return false;
        uint32_t hash = 2166136261U;
        for (std::size_t index = 0U; uid[index] != '\0'; ++index)
            hash = (hash ^ static_cast<uint8_t>(uid[index])) * 16777619U;
        for (std::size_t index = 0U; index < digestSize; ++index)
            digest[index] = static_cast<uint8_t>(
                hash >> ((index % sizeof(hash)) * 8U));
        return true;
    }
};

void makeDigest(const TestBadgeHasher& hasher, const char* uid,
                uint8_t* digest)
{
    TEST_ASSERT_TRUE(hasher.hashBadge(uid, digest,
                                      services::IBadgeHasher::DigestSize));
}

void seedLegacyWhitelist(test::MockNvsDriver& nvs,
                         const TestBadgeHasher& hasher)
{
    uint8_t blob[sizeof(uint32_t) +
                 services::StorageService::MaxBadges *
                     services::IBadgeHasher::DigestSize]{};
    const uint32_t count = 2U;
    std::memcpy(blob, &count, sizeof(count));
    makeDigest(hasher, "91:51:E2:06", blob + sizeof(count));
    makeDigest(hasher, "badge-new", blob + sizeof(count) +
        services::IBadgeHasher::DigestSize);
    TEST_ASSERT_TRUE(nvs.open(security::NVS_NAMESPACE, false));
    TEST_ASSERT_TRUE(nvs.putBlob(security::NVS_KEY_RFID_WHITELIST,
                                 blob, sizeof(blob)));
    nvs.close();
}

void testLegacyFormatMigratesAndRemovesSharedBadge()
{
    test::MockNvsDriver nvs;
    TestBadgeHasher hasher;
    seedLegacyWhitelist(nvs, hasher);
    services::StorageService storage(nvs, hasher, nullptr, true);

    TEST_ASSERT_TRUE(storage.start());
    TEST_ASSERT_FALSE(storage.isBadgeAuthorized("91:51:E2:06"));
    TEST_ASSERT_TRUE(storage.isBadgeAuthorized("badge-new"));

    TestVersionedBadgeWhitelist versioned{};
    TEST_ASSERT_TRUE(nvs.open(security::NVS_NAMESPACE, true));
    TEST_ASSERT_TRUE(nvs.getBlob(security::NVS_KEY_RFID_WHITELIST,
                                 &versioned, sizeof(versioned)));
    nvs.close();
    TEST_ASSERT_EQUAL_UINT32(security::NVS_RFID_WHITELIST_MAGIC,
                             versioned.magic);
    TEST_ASSERT_EQUAL_UINT16(security::NVS_RFID_WHITELIST_VERSION,
                             versioned.version);
    TEST_ASSERT_EQUAL_UINT16(1U, versioned.count);
}

void testVersionedFormatSurvivesRepeatedMigration()
{
    test::MockNvsDriver nvs;
    TestBadgeHasher hasher;
    seedLegacyWhitelist(nvs, hasher);

    services::StorageService firstBoot(nvs, hasher, nullptr, true);
    TEST_ASSERT_TRUE(firstBoot.start());
    firstBoot.stop();
    services::StorageService secondBoot(nvs, hasher, nullptr, true);
    TEST_ASSERT_TRUE(secondBoot.start());
    TEST_ASSERT_FALSE(secondBoot.isBadgeAuthorized("91:51:E2:06"));
    TEST_ASSERT_TRUE(secondBoot.isBadgeAuthorized("badge-new"));
}

void testNewFormatLoadsWithoutRewritingOrAddingBadges()
{
    test::MockNvsDriver nvs;
    TestBadgeHasher hasher;
    TestVersionedBadgeWhitelist versioned{};
    versioned.magic = security::NVS_RFID_WHITELIST_MAGIC;
    versioned.version = security::NVS_RFID_WHITELIST_VERSION;
    versioned.count = 1U;
    makeDigest(hasher, "badge-new", versioned.badges[0]);
    TEST_ASSERT_TRUE(nvs.open(security::NVS_NAMESPACE, false));
    TEST_ASSERT_TRUE(nvs.putBlob(security::NVS_KEY_RFID_WHITELIST,
                                 &versioned, sizeof(versioned)));
    nvs.close();

    services::StorageService storage(nvs, hasher, nullptr, true);
    TEST_ASSERT_TRUE(storage.start());
    TEST_ASSERT_TRUE(storage.isBadgeAuthorized("badge-new"));
    TEST_ASSERT_FALSE(storage.isBadgeAuthorized("91:51:E2:06"));
}

void testEmptyNvsRemainsWithoutAuthorizedBadges()
{
    test::MockNvsDriver nvs;
    TestBadgeHasher hasher;
    services::StorageService storage(nvs, hasher, nullptr, true);

    TEST_ASSERT_TRUE(storage.start());
    TEST_ASSERT_FALSE(storage.isBadgeAuthorized("91:51:E2:06"));
    TEST_ASSERT_FALSE(storage.isBadgeAuthorized("badge-new"));
}

void testMigrationDisabledByDefaultPreservesLegacyFormat()
{
    test::MockNvsDriver nvs;
    TestBadgeHasher hasher;
    seedLegacyWhitelist(nvs, hasher);
    services::StorageService storage(nvs, hasher);

    TEST_ASSERT_FALSE(security::NVS_BADGE_MIGRATION_ENABLED);
    TEST_ASSERT_TRUE(storage.start());
    TEST_ASSERT_FALSE(storage.isBadgeAuthorized("91:51:E2:06"));
    TEST_ASSERT_TRUE(storage.isBadgeAuthorized("badge-new"));

    uint8_t legacy[sizeof(uint32_t) +
                   services::StorageService::MaxBadges *
                       services::IBadgeHasher::DigestSize]{};
    TEST_ASSERT_TRUE(nvs.open(security::NVS_NAMESPACE, true));
    TEST_ASSERT_TRUE(nvs.getBlob(security::NVS_KEY_RFID_WHITELIST,
                                 legacy, sizeof(legacy)));
    nvs.close();
    uint32_t count = 0U;
    std::memcpy(&count, legacy, sizeof(count));
    TEST_ASSERT_EQUAL_UINT32(1U, count);
}

}  // namespace

void setUp() {}
void tearDown() {}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(testLegacyFormatMigratesAndRemovesSharedBadge);
    RUN_TEST(testVersionedFormatSurvivesRepeatedMigration);
    RUN_TEST(testNewFormatLoadsWithoutRewritingOrAddingBadges);
    RUN_TEST(testEmptyNvsRemainsWithoutAuthorizedBadges);
    RUN_TEST(testMigrationDisabledByDefaultPreservesLegacyFormat);
    return UNITY_END();
}
