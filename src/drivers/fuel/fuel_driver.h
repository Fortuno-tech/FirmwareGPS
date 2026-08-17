/**
 * @file    fuel_driver.h
 * @brief   Driver sonde carburant (ADC via pont diviseur) — Fortico.
 *
 * @details Utilise AdcHal::readAvg() pour le sur-échantillonnage.
 *          Le filtrage EMA est appliqué dans ce driver (couche Driver,
 *          pas dans le HAL).
 *          GPIO et paramètres hw via pin_config.h / hardware_config.h.
 */
#pragma once

#include <cstdint>

#include "interfaces/idriver.h"
#include "interfaces/ifuel_driver.h"
#include "hal/adc/adc_hal.h"
#include "constants/errors.h"

namespace fortico {
namespace drivers {

class FuelDriver :
    public interfaces::IDriver,
    public interfaces::IFuelDriver
{
public:
    explicit FuelDriver(hal::AdcHal& adcHal);

    // -------------------------------------------------------------------------
    // IDriver — cycle de vie
    // -------------------------------------------------------------------------
    constants::ErrorCode init()           override;
    constants::ErrorCode poll()           override;
    bool                 start()          override;
    bool                 stop()           override;
    bool                 isHealthy()      const override;
    bool                 isInitialized()  const override;
    void                 reset()          override;

    // -------------------------------------------------------------------------
    // IFuelDriver — interface métier
    // -------------------------------------------------------------------------
    bool readVoltage(float& voltage) override;
    bool readRaw(float& raw)         override;

private:
    hal::AdcHal& _adcHal;

    bool     _initialized;
    bool     _healthy;
    bool     _active;
    uint8_t  _consecutiveFailures;
    float    _lastVoltage;
    float    _emaVoltage;        
};

}  // namespace drivers
}  // namespace fortico