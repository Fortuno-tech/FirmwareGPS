#pragma once

#include <cstdint>

#include "core/logger/logger.h"
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
    RfidDriver(hal::I2cHal& i2cHal, core::Logger& logger);

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
    static constexpr uint8_t RFID_PN532_I2C_ADDRESS = 0x24U;
    static constexpr uint32_t PN532_RESET_ASSERT_MS = 10U;
    static constexpr uint32_t PN532_RESET_HIGH_MS = 100U;
    static constexpr uint32_t PN532_DETECTION_TIMEOUT_MS = 3000U;
    static constexpr uint32_t PN532_DETECTION_POLL_MS = 50U;
    static constexpr uint32_t PN532_I2C_TRANSACTION_TIMEOUT_MS = 50U;

    bool waitForPn532Availability(uint32_t timeoutMs, uint32_t pollIntervalMs);
    void configurePins();
    void performHardwareReset();
    void applyPn532I2cSettings();
    bool configurePn532();

#ifdef ESP32_TARGET
    static void IRAM_ATTR irqHandler(void* context);
#endif

private:
    hal::I2cHal& _i2cHal;
    core::Logger& _logger;

#ifdef ESP32_TARGET
    Adafruit_PN532 _pn532;
    SemaphoreHandle_t _irqSemaphore;
#endif

    bool _initialized;
    bool _healthy;
    bool _active;
};

}  // namespace drivers
}  // namespace fortico