#include "hal/pwm/pwm_hal.h"

#ifdef ESP32_TARGET
#include <Arduino.h>
#endif

namespace fortico {
namespace hal {

uint8_t PwmHal::nextChannel_ = 0U;

bool PwmHal::init(uint8_t pin, uint32_t frequencyHz, uint8_t resolutionBits) {
#ifdef ESP32_TARGET
    if (initialized_ || frequencyHz == 0U || resolutionBits == 0U || resolutionBits > 16U || nextChannel_ >= 16U) {
        return false;
    }
    pin_ = pin;
    channel_ = nextChannel_++;
    if (ledcSetup(channel_, frequencyHz, resolutionBits) == 0.0) {
        return false;
    }
    ledcAttachPin(pin_, channel_);
    initialized_ = true;
    return true;
#else
    (void)pin;
    (void)frequencyHz;
    (void)resolutionBits;
    return false;
#endif
}

void PwmHal::setDuty(uint8_t pin, uint32_t duty) {
#ifdef ESP32_TARGET
    if (initialized_ && pin == pin_) {
        ledcWrite(channel_, duty);
    }
#else
    (void)pin;
    (void)duty;
#endif
}

void PwmHal::stop(uint8_t pin) {
#ifdef ESP32_TARGET
    if (initialized_ && pin == pin_) {
        ledcWrite(channel_, 0U);
        ledcDetachPin(pin_);
        initialized_ = false;
    }
#else
    (void)pin;
#endif
}

}  // namespace hal
}  // namespace fortico
