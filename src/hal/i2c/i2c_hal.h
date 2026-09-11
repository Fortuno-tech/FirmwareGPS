#pragma once

#include <cstddef>
#include <cstdint>

#ifdef ESP32_TARGET
class TwoWire;
#endif

namespace fortico {
namespace hal {

class I2cHal {
public:
    I2cHal();
    ~I2cHal();

    bool init(int sdaPin, int sclPin, uint32_t speedHz);
    bool write(uint8_t address, const uint8_t* buffer, size_t length, uint32_t timeoutMs);
    bool read(uint8_t address, uint8_t* buffer, size_t length, uint32_t timeoutMs);
    bool writeRead(uint8_t address, const uint8_t* writeBuffer, size_t writeLength,
                   uint8_t* readBuffer, size_t readLength, uint32_t timeoutMs);
    bool scan(uint8_t* foundAddresses, size_t maxAddresses, size_t* foundCount);

    bool lock(uint32_t timeoutMs);
    void unlock();

#ifdef ESP32_TARGET
    TwoWire& native();
#endif

private:
#ifdef ESP32_TARGET
    ::TwoWire* wire_ = nullptr;
    void* mutex_ = nullptr;
    bool initialized_ = false;
    int sdaPin_ = -1;
    int sclPin_ = -1;
    uint32_t speedHz_ = 0U;
#endif
};

}  // namespace hal
}  // namespace fortico
