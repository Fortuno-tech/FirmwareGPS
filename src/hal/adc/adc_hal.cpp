#include "hal/adc/adc_hal.h"

#ifdef ESP32_TARGET
#include <Arduino.h>
#endif

namespace fortico {
namespace hal {

bool AdcHal::init(uint8_t pin, uint8_t attenuationDb) {
#ifdef ESP32_TARGET
    pin_ = pin;
    switch (attenuationDb) {
        case 0U: analogSetPinAttenuation(pin_, ADC_0db); break;
        case 2U: analogSetPinAttenuation(pin_, ADC_2_5db); break;
        case 6U: analogSetPinAttenuation(pin_, ADC_6db); break;
        case 11U: analogSetPinAttenuation(pin_, ADC_11db); break;
        default: return false;
    }
    initialized_ = true;
    return true;
#else
    (void)pin;
    (void)attenuationDb;
    return false;
#endif
}

uint16_t AdcHal::readRaw() const {
#ifdef ESP32_TARGET
    return initialized_ ? static_cast<uint16_t>(analogRead(pin_)) : 0U;
#else
    return 0U;
#endif
}

uint32_t AdcHal::readMv() const {
#ifdef ESP32_TARGET
    return initialized_ ? analogReadMilliVolts(pin_) : 0U;
#else
    return 0U;
#endif
}

float AdcHal::readAvg(uint8_t samples) const {
    if (!initialized_ || samples == 0U) {
        return 0.0F;
    }

    uint32_t totalMv = 0U;
    for (uint8_t sample = 0U; sample < samples; ++sample) {
        totalMv += readMv();
    }
    return static_cast<float>(totalMv) / static_cast<float>(samples);
}

}  // namespace hal
}  // namespace fortico
