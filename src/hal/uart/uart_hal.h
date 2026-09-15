#pragma once

#include <cstddef>
#include <cstdint>

#ifdef ESP32_TARGET
class HardwareSerial;
#endif

namespace fortico {
namespace hal {

class UartHal {
public:
    bool init(uint8_t uartNum, uint32_t baud, int txPin, int rxPin);
    size_t write(const uint8_t* data, size_t length);
    size_t read(uint8_t* buffer, size_t maxLength, uint32_t timeoutMs);
    int available() const;

private:
#ifdef ESP32_TARGET
    ::HardwareSerial* serial_ = nullptr;
#endif
};

}  // namespace hal
}  // namespace fortico
