#include <unity.h>
#include <cstring>

#include "services/security/security_service.h"
#include "services/security/provisioning_service.h"
#include "services/security/ota_key_store.h"
#include "mocks/mock_nvs_driver.h"

using namespace fortico;

namespace {

test::MockNvsDriver g_mockNvs;

}  // namespace

void setUp() {
    g_mockNvs = test::MockNvsDriver();
}

void tearDown() {}

void test_security_start_fails_when_not_provisioned() {
    services::SecurityService security(g_mockNvs);
    TEST_ASSERT_FALSE(security.start());
    TEST_ASSERT_FALSE(security.isRunning());
}

void test_security_provision_root_key_success() {
    services::SecurityService security(g_mockNvs);
    uint8_t rootKey[32] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                           0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
                           0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                           0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20};

    TEST_ASSERT_TRUE(security.provisionRootKey(rootKey, sizeof(rootKey)));
    TEST_ASSERT_TRUE(security.isRunning());
}

void test_security_provision_root_key_prevents_double_provision() {
    services::SecurityService security(g_mockNvs);
    uint8_t rootKey[32] = {0xAA};
    uint8_t rootKey2[32] = {0xBB};

    TEST_ASSERT_TRUE(security.provisionRootKey(rootKey, sizeof(rootKey)));

    // Second provisioning attempt must be rejected to prevent overwriting root key
    services::SecurityService security2(g_mockNvs);
    TEST_ASSERT_FALSE(security2.provisionRootKey(rootKey2, sizeof(rootKey2)));
}

void test_security_device_id_store_and_load() {
    services::SecurityService security(g_mockNvs);
    uint8_t rootKey[32] = {0x01};
    TEST_ASSERT_TRUE(security.provisionRootKey(rootKey, sizeof(rootKey)));

    const char* devId = "FORTICO-TEST-123";
    TEST_ASSERT_TRUE(security.storeDeviceId(devId));

    char loadedId[32] = {};
    TEST_ASSERT_TRUE(security.loadDeviceId(loadedId, sizeof(loadedId)));
    TEST_ASSERT_EQUAL_STRING(devId, loadedId);
}

void test_provisioning_service_hex_to_bytes() {
    const char* hex = "0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20";
    uint8_t bytes[32] = {};
    TEST_ASSERT_TRUE(services::ProvisioningService::hexToBytes(hex, bytes, sizeof(bytes)));
    TEST_ASSERT_EQUAL_HEX8(0x01, bytes[0]);
    TEST_ASSERT_EQUAL_HEX8(0x10, bytes[15]);
    TEST_ASSERT_EQUAL_HEX8(0x20, bytes[31]);

    // Invalid hex length
    TEST_ASSERT_FALSE(services::ProvisioningService::hexToBytes("123", bytes, sizeof(bytes)));
}

void test_ota_key_store() {
    services::OtaKeyStore otaStore(g_mockNvs);
    TEST_ASSERT_FALSE(otaStore.hasPublicKey());

    uint8_t pubKey[32] = {0xDE, 0xAD, 0xBE, 0xEF};
    TEST_ASSERT_TRUE(otaStore.storePublicKey(pubKey));
    TEST_ASSERT_TRUE(otaStore.hasPublicKey());

    uint8_t loadedKey[32] = {};
    TEST_ASSERT_TRUE(otaStore.loadPublicKey(loadedKey));
    TEST_ASSERT_EQUAL_MEMORY(pubKey, loadedKey, 32);
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_security_start_fails_when_not_provisioned);
    RUN_TEST(test_security_provision_root_key_success);
    RUN_TEST(test_security_provision_root_key_prevents_double_provision);
    RUN_TEST(test_security_device_id_store_and_load);
    RUN_TEST(test_provisioning_service_hex_to_bytes);
    RUN_TEST(test_ota_key_store);

    return UNITY_END();
}

