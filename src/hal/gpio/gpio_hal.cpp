#include "hal/gpio/gpio_hal.h"

#ifdef ESP32_TARGET
#include <Arduino.h>
#endif

namespace fortico {
namespace hal {

bool GpioHal::init(uint8_t pin, GpioMode mode) {
#ifdef ESP32_TARGET
    uint8_t arduinoMode = INPUT;
    switch (mode) {
        case GpioMode::Input: arduinoMode = INPUT; break;
        case GpioMode::InputPullup: arduinoMode = INPUT_PULLUP; break;
        case GpioMode::Output: arduinoMode = OUTPUT; break;
    }
    pinMode(pin, arduinoMode);
    return true;
#else
    (void)pin;
    (void)mode;
    return false;
#endif
}

void GpioHal::write(uint8_t pin, bool value) {
#ifdef ESP32_TARGET
    digitalWrite(pin, value ? HIGH : LOW);
#else
    (void)pin;
    (void)value;
#endif
}

bool GpioHal::read(uint8_t pin) const {
#ifdef ESP32_TARGET
    return digitalRead(pin) == HIGH;
#else
    (void)pin;
    return false;
#endif
}

bool GpioHal::attachInterrupt(uint8_t pin, GpioIsr isr, GpioIntrType type) {
#ifdef ESP32_TARGET
    if (isr == nullptr) {
        return false;
    }

    int mode = CHANGE;
    switch (type) {
        case GpioIntrType::Rising: mode = RISING; break;
        case GpioIntrType::Falling: mode = FALLING; break;
        case GpioIntrType::Change: mode = CHANGE; break;
        case GpioIntrType::Low: mode = ONLOW; break;
        case GpioIntrType::High: mode = ONHIGH; break;
    }
    ::attachInterrupt(digitalPinToInterrupt(pin), isr, mode);
    return true;
#else
    (void)pin;
    (void)isr;
    (void)type;
    return false;
#endif
}

}  // namespace hal
}  // namespace fortico
