#include "hal/uart/uart_hal.h"

#ifdef ESP32_TARGET
#include <Arduino.h>
#endif

namespace fortico {
namespace hal {

bool UartHal::init(uint8_t uartNum, uint32_t baud, int txPin, int rxPin) {
#ifdef ESP32_TARGET
    switch (uartNum) {
        case 0: serial_ = &Serial; break;
        case 1: serial_ = &Serial1; break;
        case 2: serial_ = &Serial2; break;
        default: return false;
    }
    serial_->begin(baud, SERIAL_8N1, rxPin, txPin);
    return true;
#else
    (void)uartNum;
    (void)baud;
    (void)txPin;
    (void)rxPin;
    return false;
#endif
}

size_t UartHal::write(const uint8_t* data, size_t length) {
#ifdef ESP32_TARGET
    return serial_ == nullptr || data == nullptr ? 0U : serial_->write(data, length);
#else
    (void)data;
    (void)length;
    return 0U;
#endif
}

size_t UartHal::read(uint8_t* buffer, size_t maxLength, uint32_t timeoutMs) {
#ifdef ESP32_TARGET
    if (serial_ == nullptr || buffer == nullptr || maxLength == 0U) {
        return 0U;
    }
    serial_->setTimeout(timeoutMs);
    return serial_->readBytes(reinterpret_cast<char*>(buffer), maxLength);
#else
    (void)buffer;
    (void)maxLength;
    (void)timeoutMs;
    return 0U;
#endif
}

int UartHal::available() const {
#ifdef ESP32_TARGET
    return serial_ == nullptr ? 0 : serial_->available();
#else
    return 0;
#endif
}

}  // namespace hal
}  // namespace fortico
