#pragma once

#include <cstdint>

#include "constants/errors.h"
#include "hal/i2c/i2c_hal.h"
#include "interfaces/irfid_driver.h"

#ifdef ESP32_TARGET
#include <Adafruit_PN532.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#endif

namespace fortico {
namespace drivers {

class RfidDriver : public interfaces::IRfidDriver {
public:

    explicit RfidDriver(hal::I2cHal& i2cHal);

    constants::ErrorCode init() override;
    constants::ErrorCode poll() override;

    bool start() override;
    bool stop() override;

    bool isHealthy() const override;
    bool isInitialized() const override;

    void reset() override;

    bool readUid(
        uint8_t* uid,
        uint8_t& uidLength,
        uint32_t timeoutMs) override;

private:

    // Adresse I2C du PN532
    static constexpr uint8_t RFID_PN532_I2C_ADDRESS = 0x24U;

    // Reset PN532
    static constexpr uint32_t PN532_RESET_ASSERT_MS = 10U;
    static constexpr uint32_t PN532_RESET_LOW_MS = 100U;
    static constexpr uint32_t PN532_RESET_HIGH_MS = 500U;

    // Détection PN532
    static constexpr uint32_t PN532_DETECTION_TIMEOUT_MS = 5000U;
    static constexpr uint32_t PN532_DETECTION_POLL_MS = 100U;
    static constexpr uint32_t PN532_I2C_TRANSACTION_TIMEOUT_MS = 200U;
    static constexpr uint8_t PN532_PASSIVE_ACTIVATION_RETRIES = 0x01U;
    static constexpr uint32_t PN532_COMMAND_TIMEOUT_MS = 100U;
    static constexpr uint32_t PN532_IDLE_POLL_INTERVAL_MS = 1000U;
    static constexpr uint8_t PN532_INVALID_STATE_ERROR = 0x15U;

    bool waitForPn532Availability(
        uint32_t timeoutMs,
        uint32_t pollIntervalMs);

    void configurePins();

    void performHardwareReset();

    void applyPn532I2cSettings();

    bool configurePn532();

#ifdef ESP32_TARGET

    bool startPassiveTargetDetection();

    bool readPassiveTargetResponse(
        uint8_t* uid,
        uint8_t& uidLength);

    static void IRAM_ATTR irqHandler(void* context);

    static void taskEntry(void* context);

    void runTask();

    bool readHardwareUid(
        uint8_t* uid,
        uint8_t& uidLength);

#endif

private:

    // HAL I2C
    hal::I2cHal& _i2cHal;

#ifdef ESP32_TARGET

    // PN532
    Adafruit_PN532 _pn532;

    // FreeRTOS
    TaskHandle_t _taskHandle;
    SemaphoreHandle_t _resultMutex;

    volatile bool _taskRunning;
    volatile bool _taskStarted;
    bool _passiveDetectionStarted;
    bool _awaitingDetectionAck;
    volatile bool _irqPending;
    bool _irqAttached;
    uint8_t _lastDetectionError;
    uint32_t _detectionStartCount;
    uint32_t _irqWaitCount;
    uint32_t _responseReadCount;
    uint32_t _lastReadAttemptMs;

    // Résultat de lecture
    uint8_t _resultUid[16];
    uint8_t _resultUidLength;
    bool _resultAvailable;

#endif

    // État driver
    bool _initialized;
    bool _healthy;
    bool _active;
};

}  // namespace drivers
}  // namespace fortico
