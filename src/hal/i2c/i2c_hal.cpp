#include "hal/i2c/i2c_hal.h"

#include "config/hardware_config.h"

#ifdef ESP32_TARGET
#include <Arduino.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

namespace fortico {
namespace hal {

I2cHal::I2cHal() {
#ifdef ESP32_TARGET
    mutex_ = xSemaphoreCreateRecursiveMutex();
#endif
}

I2cHal::~I2cHal() {
#ifdef ESP32_TARGET
    if (mutex_ != nullptr) {
        vSemaphoreDelete(static_cast<SemaphoreHandle_t>(mutex_));
        mutex_ = nullptr;
    }
#endif
}

bool I2cHal::lock(uint32_t timeoutMs) {
#ifdef ESP32_TARGET
    if (mutex_ == nullptr) {
        return false;
    }
    const TickType_t ticks = (timeoutMs == portMAX_DELAY) ? portMAX_DELAY : pdMS_TO_TICKS(timeoutMs);
    return xSemaphoreTakeRecursive(static_cast<SemaphoreHandle_t>(mutex_), ticks) == pdTRUE;
#else
    (void)timeoutMs;
    return true;
#endif
}

void I2cHal::unlock() {
#ifdef ESP32_TARGET
    if (mutex_ != nullptr) {
        xSemaphoreGiveRecursive(static_cast<SemaphoreHandle_t>(mutex_));
    }
#endif
}

bool I2cHal::init(int sdaPin, int sclPin, uint32_t speedHz) {
#ifdef ESP32_TARGET
    if (!lock(50U)) {
        return false;
    }

    // Wire est global : le HAL en est l'unique proprietaire et evite toute
    // reinitialisation pendant l'execution des services.
    if (initialized_) {
        const bool matches = sdaPin_ == sdaPin &&
                             sclPin_ == sclPin &&
                             speedHz_ == speedHz;
        unlock();
        return matches;
    }

    wire_ = &Wire;
    const bool ok = wire_->begin(sdaPin, sclPin, speedHz);
    if (ok) {
        wire_->setClock(speedHz);
        wire_->setTimeOut(hardware::I2C_TRANSACTION_MAX_MS);
        sdaPin_ = sdaPin;
        sclPin_ = sclPin;
        speedHz_ = speedHz;
        initialized_ = true;
    }
    unlock();
    return ok;
#else
    (void)sdaPin;
    (void)sclPin;
    (void)speedHz;
    return false;
#endif
}

bool I2cHal::write(uint8_t address, const uint8_t* buffer, size_t length, uint32_t timeoutMs) {
#ifdef ESP32_TARGET
    if (wire_ == nullptr || (buffer == nullptr && length != 0U)) {
        return false;
    }
    if (!lock(timeoutMs)) {
        return false;
    }
    const uint32_t effectiveTimeout = timeoutMs < 100U ? 100U : timeoutMs;
    wire_->setTimeOut(effectiveTimeout);
    wire_->beginTransmission(address);
    if (length != 0U && wire_->write(buffer, length) != length) {
        wire_->endTransmission();
        unlock();
        return false;
    }
    const bool ok = (wire_->endTransmission() == 0);
    unlock();
    return ok;
#else
    (void)address;
    (void)buffer;
    (void)length;
    (void)timeoutMs;
    return false;
#endif
}

bool I2cHal::read(uint8_t address, uint8_t* buffer, size_t length, uint32_t timeoutMs) {
#ifdef ESP32_TARGET
    if (wire_ == nullptr || (buffer == nullptr && length != 0U)) {
        return false;
    }
    if (!lock(timeoutMs)) {
        Serial.println(
            "[I2C][DBG] I2cHal::read: lock failed");
        return false;
    }
    const uint32_t effectiveTimeout = timeoutMs < 100U ? 100U : timeoutMs;
    wire_->setTimeOut(effectiveTimeout);
    const size_t received = wire_->requestFrom(static_cast<int>(address), static_cast<int>(length));
    if (received != length) {
        Serial.printf(
            "[I2C][DBG] I2cHal::read: requestFrom mismatch, address=0x%02X requested=%zu received=%zu\n",
            address,
            length,
            received);
        unlock();
        return false;
    }
    for (size_t index = 0; index < length; ++index) {
        if (wire_->available() == 0) {
            Serial.printf(
                "[I2C][DBG] I2cHal::read: available() became 0 before reading byte %zu, address=0x%02X\n",
                index,
                address);
            unlock();
            return false;
        }
        buffer[index] = static_cast<uint8_t>(wire_->read());
    }
    unlock();
    return true;
#else
    (void)address;
    (void)buffer;
    (void)length;
    (void)timeoutMs;
    return false;
#endif
}

bool I2cHal::writeRead(uint8_t address, const uint8_t* writeBuffer, size_t writeLength,
                       uint8_t* readBuffer, size_t readLength, uint32_t timeoutMs) {
#ifdef ESP32_TARGET
    if (wire_ == nullptr || readBuffer == nullptr || readLength == 0U) {
        return false;
    }
    if (!lock(timeoutMs)) {
        Serial.println(
            "[I2C][DBG] I2cHal::writeRead: lock failed");
        return false;
    }
    const uint32_t effectiveTimeout = timeoutMs < 100U ? 100U : timeoutMs;
    wire_->setTimeOut(effectiveTimeout);
    if (writeBuffer != nullptr && writeLength > 0U) {
        wire_->beginTransmission(address);
        if (wire_->write(writeBuffer, writeLength) != writeLength) {
            wire_->endTransmission();
            Serial.printf(
                "[I2C][DBG] I2cHal::writeRead: write length mismatch, address=0x%02X\n",
                address);
            unlock();
            return false;
        }
        // endTransmission(false) = Repeated Start obligatoire pour la lecture de registre
        if (wire_->endTransmission(false) != 0) {
            Serial.printf(
                "[I2C][DBG] I2cHal::writeRead: repeated start failed, address=0x%02X\n",
                address);
            unlock();
            return false;
        }
    }
    const size_t received = wire_->requestFrom(static_cast<int>(address), static_cast<int>(readLength));
    if (received != readLength) {
        Serial.printf(
            "[I2C][DBG] I2cHal::writeRead: requestFrom mismatch, address=0x%02X requested=%zu received=%zu\n",
            address,
            readLength,
            received);
        unlock();
        return false;
    }
    for (size_t index = 0; index < readLength; ++index) {
        if (wire_->available() == 0) {
            Serial.printf(
                "[I2C][DBG] I2cHal::writeRead: available() became 0 before reading byte %zu, address=0x%02X\n",
                index,
                address);
            unlock();
            return false;
        }
        readBuffer[index] = static_cast<uint8_t>(wire_->read());
    }
    unlock();
    return true;
#else
    (void)address;
    (void)writeBuffer;
    (void)writeLength;
    (void)readBuffer;
    (void)readLength;
    (void)timeoutMs;
    return false;
#endif
}

bool I2cHal::scan(uint8_t* foundAddresses, size_t maxAddresses, size_t* foundCount) {
#ifdef ESP32_TARGET
    if (wire_ == nullptr || foundCount == nullptr) {
        return false;
    }
    if (!lock(100U)) {
        return false;
    }
    *foundCount = 0U;
    for (uint8_t address = 1U; address < 127U; ++address) {
        wire_->beginTransmission(address);
        if (wire_->endTransmission() == 0) {
            if (*foundCount >= maxAddresses || foundAddresses == nullptr) {
                unlock();
                return false;
            }
            foundAddresses[*foundCount] = address;
            ++(*foundCount);
        }
    }
    unlock();
    return true;
#else
    (void)foundAddresses;
    (void)maxAddresses;
    if (foundCount != nullptr) {
        *foundCount = 0U;
    }
    return false;
#endif
}

#ifdef ESP32_TARGET
TwoWire& I2cHal::native() {
    return wire_ != nullptr ? *wire_ : Wire;
}
#endif

}  // namespace hal
}  // namespace fortico
