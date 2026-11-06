#include <unity.h>

#include "config/ota_config.h"
#include "config/project_config.h"
#include "managers/ota_manager/ota_manager.h"

#if !defined(FORTICO_PRODUCTION_BUILD)
#error "OTA policy test must compile with the production profile"
#endif

namespace {

class FakeOtaBackend final : public fortico::managers::IOtaBackend {
public:
    bool begin(std::size_t) override { ++beginCalls; return true; }
    bool write(const uint8_t*, std::size_t) override { return true; }
    bool finish() override { return true; }
    void abort() override {}
    unsigned int beginCalls = 0U;
};

class FakeSignatureVerifier final
    : public fortico::managers::IOtaSignatureVerifier {
public:
    bool begin(std::size_t) override { ++beginCalls; return true; }
    bool update(const uint8_t*, std::size_t) override { return true; }
    bool verify(const uint8_t*, std::size_t) override { return true; }
    void reset() override {}
    unsigned int beginCalls = 0U;
};

}  // namespace

void setUp() {}
void tearDown() {}

void test_ota_is_disabled_for_production_profile() {
    TEST_ASSERT_FALSE(fortico::ota::UPDATE_PIPELINE_COMPLETE);
    TEST_ASSERT_FALSE(fortico::ota::ENABLED_IN_PRODUCTION);
    TEST_ASSERT_FALSE(fortico::project::FEATURE_OTA_ENABLED);
}

void test_ota_manager_rejects_update_before_side_effects()
{
    FakeOtaBackend backend;
    FakeSignatureVerifier verifier;
    fortico::managers::OtaManager manager(backend, verifier);
    manager.init();

    TEST_ASSERT_FALSE(manager.begin(1024U));
    TEST_ASSERT_EQUAL_UINT32(0U, backend.beginCalls);
    TEST_ASSERT_EQUAL_UINT32(0U, verifier.beginCalls);
    TEST_ASSERT_FALSE(manager.inProgress());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_ota_is_disabled_for_production_profile);
    RUN_TEST(test_ota_manager_rejects_update_before_side_effects);
    return UNITY_END();
}
