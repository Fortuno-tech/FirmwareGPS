#include "drivers/rfid/rfid_driver.h"

#include <Arduino.h>
#include <cstdio>
#include <cstring>

#include "config/hardware_config.h"
#include "config/logging_config.h"
#include "config/pin_config.h"

namespace fortico {
namespace drivers {

/* ============================================================
 * CONSTRUCTEUR
 * ============================================================ */

RfidDriver::RfidDriver(hal::I2cHal& i2cHal, core::Logger& logger)
    : _i2cHal(i2cHal), _logger(logger)
#ifdef ESP32_TARGET
    , _pn532(pins::PN532_IRQ, pins::PN532_RESET, &i2cHal.native())
    , _irqSemaphore(nullptr)
#endif
    , _initialized(false)
    , _healthy(false)
    , _active(false)
{
}

/* ============================================================
 * GESTIONNAIRE D'INTERRUPTION (ISR)
 * ============================================================ */

void IRAM_ATTR RfidDriver::irqHandler(void* context)
{
    RfidDriver* const driver = static_cast<RfidDriver*>(context);
    if (driver != nullptr && driver->_irqSemaphore != nullptr) {
        BaseType_t higherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(driver->_irqSemaphore, &higherPriorityTaskWoken);
        if (higherPriorityTaskWoken == pdTRUE) {
            portYIELD_FROM_ISR();
        }
    }
}

bool RfidDriver::waitForPn532Availability(uint32_t timeoutMs, uint32_t pollIntervalMs)
{
    const uint32_t startTimeMs = millis();
    TwoWire& wire = _i2cHal.native();

    wire.setClock(hardware::I2C_FREQUENCY_HZ);
    wire.setTimeOut(PN532_I2C_TRANSACTION_TIMEOUT_MS);

    while ((millis() - startTimeMs) < timeoutMs) {
        wire.beginTransmission(RFID_PN532_I2C_ADDRESS);
        if (wire.endTransmission() == 0U) {
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(pollIntervalMs));
    }
    return false;
}

void RfidDriver::configurePins()
{
    pinMode(pins::PN532_RESET, OUTPUT);
    digitalWrite(pins::PN532_RESET, HIGH);
    pinMode(pins::PN532_IRQ, INPUT_PULLUP);
}

void RfidDriver::performHardwareReset()
{
    configurePins();
    digitalWrite(pins::PN532_RESET, LOW);
    vTaskDelay(pdMS_TO_TICKS(PN532_RESET_ASSERT_MS));
    digitalWrite(pins::PN532_RESET, HIGH);
    vTaskDelay(pdMS_TO_TICKS(PN532_RESET_HIGH_MS));
}

void RfidDriver::applyPn532I2cSettings()
{
    _i2cHal.native().setClock(hardware::I2C_FREQUENCY_HZ);
    _i2cHal.native().setTimeOut(PN532_I2C_TRANSACTION_TIMEOUT_MS);
}

bool RfidDriver::configurePn532()
{
    if (!_i2cHal.lock(hardware::I2C_MUTEX_TIMEOUT_MS)) {
        _logger.error(logging::TAG_RFID, "Mutex I2C indisponible lors de la configuration");
        return false;
    }

    applyPn532I2cSettings();

    if (!_pn532.begin()) {
        _i2cHal.unlock();
        _logger.error(logging::TAG_RFID, "PN532 begin() a echoue");
        return false;
    }

    pinMode(pins::PN532_IRQ, INPUT_PULLUP);

    applyPn532I2cSettings();
    const uint32_t firmwareVersion = _pn532.getFirmwareVersion();
    if (firmwareVersion == 0U) {
        _i2cHal.unlock();
        _logger.error(logging::TAG_RFID, "Firmware PN532 non detecte");
        return false;
    }

    _logger.info(logging::TAG_RFID, "PN532 firmware: 0x%08lX", static_cast<unsigned long>(firmwareVersion));

    applyPn532I2cSettings();
    if (!_pn532.SAMConfig()) {
        _i2cHal.unlock();
        _logger.error(logging::TAG_RFID, "SAMConfig() a echoue");
        return false;
    }

    applyPn532I2cSettings();
    _pn532.startPassiveTargetIDDetection(PN532_MIFARE_ISO14443A);

    _i2cHal.unlock();

    vTaskDelay(pdMS_TO_TICKS(50U));

    _logger.info(logging::TAG_RFID, "PN532 configure et en ecoute passive");
    return true;
}

constants::ErrorCode RfidDriver::init()
{
    if (_initialized) return constants::ErrorCode::OK;

#ifdef ESP32_TARGET
    _logger.info(logging::TAG_RFID, "Initialisation I2C...");

    if (!_i2cHal.init(pins::I2C_SDA, pins::I2C_SCL, hardware::I2C_FREQUENCY_HZ)) {
        _logger.error(logging::TAG_RFID, "Initialisation I2C echouee");
        return constants::ErrorCode::I2C_BUS_TIMEOUT;
    }

    configurePins();

    if (_irqSemaphore == nullptr) {
        _irqSemaphore = xSemaphoreCreateBinary();
    }

    attachInterruptArg(pins::PN532_IRQ, &RfidDriver::irqHandler, this, FALLING);

    if (!waitForPn532Availability(PN532_DETECTION_TIMEOUT_MS, PN532_DETECTION_POLL_MS)) {
        _logger.error(logging::TAG_RFID, "PN532 absent sur le bus I2C");
        return constants::ErrorCode::RFID_FAILURE;
    }

    _logger.info(logging::TAG_RFID, "PN532 detecte a l'adresse I2C 0x24");

    if (!configurePn532()) {
        return constants::ErrorCode::RFID_FAILURE;
    }
#else
    return constants::ErrorCode::RFID_FAILURE;
#endif

    _initialized = true;
    _healthy = true;
    _active = true;
    return constants::ErrorCode::OK;
}

constants::ErrorCode RfidDriver::poll()
{
    if (!_initialized) return constants::ErrorCode::NOT_INITIALIZED;
    if (!_healthy) return constants::ErrorCode::RFID_FAILURE;
    return constants::ErrorCode::OK;
}

bool RfidDriver::start() { _active = true; return true; }
bool RfidDriver::stop() { _active = false; return true; }
bool RfidDriver::isHealthy() const { return _initialized && _healthy; }
bool RfidDriver::isInitialized() const { return _initialized; }

void RfidDriver::reset()
{
    _active = false;
    _healthy = false;
#ifdef ESP32_TARGET
    performHardwareReset();
    if (configurePn532()) {
        _healthy = true;
        _active = true;
        _logger.info(logging::TAG_RFID, "PN532 réinitialisé avec succès");
    } else {
        _logger.error(logging::TAG_RFID, "Echec de la réinitialisation du PN532");
    }
#endif
}

bool RfidDriver::readUid(uint8_t* uid, uint8_t& uidLength, uint32_t timeoutMs)
{
    uidLength = 0U;
    if (!_initialized || !_active || !_healthy || uid == nullptr || _irqSemaphore == nullptr) {
        return false;
    }

#ifdef ESP32_TARGET
    if (xSemaphoreTake(_irqSemaphore, pdMS_TO_TICKS(timeoutMs)) != pdTRUE) {
        return false;
    }

    if (!_i2cHal.lock(hardware::I2C_MUTEX_TIMEOUT_MS)) {
        return false;
    }

    applyPn532I2cSettings();
    const bool success = _pn532.readDetectedPassiveTargetID(uid, &uidLength);

    applyPn532I2cSettings();
    _pn532.startPassiveTargetIDDetection(PN532_MIFARE_ISO14443A);

    _i2cHal.unlock();

    return success && (uidLength > 0U);
#else
    (void)timeoutMs;
    return false;
#endif
}

}  // namespace drivers
}  // namespace fortico