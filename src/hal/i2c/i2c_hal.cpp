#include "hal/i2c/i2c_hal.h"

#ifdef ESP32_TARGET
#include <Arduino.h>
#include <Wire.h>
#endif

namespace fortico {
namespace hal {

bool I2cHal::init(int sdaPin, int sclPin, uint32_t speedHz) {
#ifdef ESP32_TARGET
    wire_ = &Wire;
    return wire_->begin(sdaPin, sclPin, speedHz);
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
    wire_->setTimeOut(timeoutMs);
    wire_->beginTransmission(address);
    if (length != 0U && wire_->write(buffer, length) != length) {
        wire_->endTransmission();
        return false;
    }
    return wire_->endTransmission() == 0;
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
    wire_->setTimeOut(timeoutMs);
    const size_t received = wire_->requestFrom(static_cast<int>(address), static_cast<int>(length));
    if (received != length) {
        return false;
    }
    for (size_t index = 0; index < length; ++index) {
        if (wire_->available() == 0) {
            return false;
        }
        buffer[index] = static_cast<uint8_t>(wire_->read());
    }
    return true;
#else
    (void)address;
    (void)buffer;
    (void)length;
    (void)timeoutMs;
    return false;
#endif
}

bool I2cHal::scan(uint8_t* foundAddresses, size_t maxAddresses, size_t* foundCount) {
#ifdef ESP32_TARGET
    if (wire_ == nullptr || foundCount == nullptr) {
        return false;
    }
    *foundCount = 0U;
    for (uint8_t address = 1U; address < 127U; ++address) {
        wire_->beginTransmission(address);
        if (wire_->endTransmission() == 0) {
            if (*foundCount >= maxAddresses || foundAddresses == nullptr) {
                return false;
            }
            foundAddresses[*foundCount] = address;
            ++(*foundCount);
        }
    }
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

}  // namespace hal
}  // namespace fortico
