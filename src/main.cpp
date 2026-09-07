/**
 * @file main.cpp
 * @brief Test matériel NvsDriver + NvsHal sur ESP32.
 */

#include <Arduino.h>

#include "hal/nvs/nvs_hal.h"
#include "drivers/nvs/nvs_driver.h"
#include "constants/errors.h"

using namespace fortico;

#define PASS(msg) \
    Serial.printf("[PASS] %s\n", msg)

#define FAIL(msg) \
    Serial.printf("[FAIL] %s\n", msg)

#define CHECK(condition, msg) \
    do { \
        if (condition) PASS(msg); \
        else FAIL(msg); \
    } while (0)

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("========================================");
    Serial.println(" TEST NVS DRIVER - FORTICO ESP32");
    Serial.println("========================================");

    hal::NvsHal nvsHal;
    drivers::NvsDriver driver(nvsHal);

    // ================================================================
    // 1. Etat initial
    // ================================================================

    Serial.println();
    Serial.println("[1] Etat initial");
    Serial.println("----------------------------------------");

    CHECK(!driver.isInitialized(),
          "Driver non initialise");

    CHECK(!driver.isHealthy(),
          "Driver non sain");

    // ================================================================
    // 2. Initialisation
    // ================================================================

    Serial.println();
    Serial.println("[2] Initialisation");
    Serial.println("----------------------------------------");

    const auto initResult = driver.init();

    CHECK(initResult == constants::ErrorCode::OK,
          "NvsDriver::init()");

    CHECK(driver.isInitialized(),
          "Driver initialise");

    CHECK(driver.isHealthy(),
          "Driver sain");

    // ================================================================
    // 3. Ouverture namespace
    // ================================================================

    Serial.println();
    Serial.println("[3] Ouverture namespace");
    Serial.println("----------------------------------------");

    CHECK(driver.open("fortico_test", false),
          "open(fortico_test, readWrite)");

    // ================================================================
    // 4. putString / getString
    // ================================================================

    Serial.println();
    Serial.println("[4] Test putString() / getString()");
    Serial.println("----------------------------------------");

    const char* expectedString =
        "Fortico NvsDriver ESP32";

    CHECK(driver.putString("test_string", expectedString),
          "putString()");

    char buffer[64] = {};

    CHECK(driver.getString(
              "test_string",
              buffer,
              sizeof(buffer)),
          "getString()");

    CHECK(strcmp(buffer, expectedString) == 0,
          "Verification chaine");

    Serial.print("Valeur lue : ");
    Serial.println(buffer);

    // ================================================================
    // 5. putUInt32 / getUInt32
    // ================================================================

    Serial.println();
    Serial.println("[5] Test putUInt32() / getUInt32()");
    Serial.println("----------------------------------------");

    const uint32_t expectedValue = 123456789UL;

    CHECK(driver.putUInt32(
              "test_uint32",
              expectedValue),
          "putUInt32()");

    uint32_t readValue = 0U;

    CHECK(driver.getUInt32(
              "test_uint32",
              readValue),
          "getUInt32()");

    CHECK(readValue == expectedValue,
          "Verification uint32");

    Serial.print("Valeur lue : ");
    Serial.println(readValue);

    // ================================================================
    // 6. putBlob / getBlob
    // ================================================================

    Serial.println();
    Serial.println("[6] Test putBlob() / getBlob()");
    Serial.println("----------------------------------------");

    const uint8_t txBlob[] = {
        0x01, 0x02, 0x03, 0x04,
        0x05, 0x06, 0x07, 0x08
    };

    uint8_t rxBlob[sizeof(txBlob)] = {};

    CHECK(driver.putBlob(
              "test_blob",
              txBlob,
              sizeof(txBlob)),
          "putBlob()");

    CHECK(driver.getBlob(
              "test_blob",
              rxBlob,
              sizeof(rxBlob)),
          "getBlob()");

    CHECK(memcmp(
              txBlob,
              rxBlob,
              sizeof(txBlob)) == 0,
          "Verification blob");

    // ================================================================
    // 7. Etat / poll
    // ================================================================

    Serial.println();
    Serial.println("[7] Etat du Driver");
    Serial.println("----------------------------------------");

    CHECK(driver.isHealthy(),
          "Driver toujours sain");

    CHECK(driver.poll() == constants::ErrorCode::OK,
          "poll() retourne OK");

    // ================================================================
    // 8. remove
    // ================================================================

    Serial.println();
    Serial.println("[8] Test remove()");
    Serial.println("----------------------------------------");

    CHECK(driver.remove("test_string"),
          "remove()");

    char removedBuffer[64] = {};

    CHECK(!driver.getString(
              "test_string",
              removedBuffer,
              sizeof(removedBuffer)),
          "Lecture apres remove() doit echouer");

    // ================================================================
    // 9. Fermeture
    // ================================================================

    Serial.println();
    Serial.println("[9] Fermeture namespace");
    Serial.println("----------------------------------------");

    driver.close();

    CHECK(!driver.putString(
              "after_close",
              "FAIL"),
          "Ecriture apres close() doit echouer");

    // ================================================================
    // 10. Stop
    // ================================================================

    Serial.println();
    Serial.println("[10] Stop");
    Serial.println("----------------------------------------");

    CHECK(driver.stop(),
          "stop()");

    CHECK(!driver.isHealthy(),
          "Driver non sain apres stop");

    // ================================================================
    // 11. Protection après stop
    // ================================================================

    Serial.println();
    Serial.println("[11] Protection apres stop");
    Serial.println("----------------------------------------");

    CHECK(!driver.open("fortico_test", false),
          "open() apres stop doit echouer");

    // ================================================================
    // 12. Restart
    // ================================================================

    Serial.println();
    Serial.println("[12] Restart");
    Serial.println("----------------------------------------");

    CHECK(driver.start(),
          "start()");

    CHECK(driver.isHealthy(),
          "Driver sain apres start");

    // ================================================================
    // 13. Reset
    // ================================================================

    Serial.println();
    Serial.println("[13] Reset");
    Serial.println("----------------------------------------");

    driver.reset();

    CHECK(driver.isInitialized(),
          "Driver toujours initialise apres reset");

    // ================================================================
    // Nettoyage
    // ================================================================

    Serial.println();
    Serial.println("[14] Nettoyage");
    Serial.println("----------------------------------------");

    if (driver.start()) {

        if (driver.open("fortico_test", false)) {

            driver.remove("test_uint32");
            driver.remove("test_blob");

            driver.close();
        }

        driver.stop();
    }

    Serial.println();
    Serial.println("========================================");
    Serial.println("      TEST NVS DRIVER TERMINE");
    Serial.println("========================================");
}

void loop()
{
    delay(1000);
}