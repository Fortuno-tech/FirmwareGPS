/**
 * @file    fuel_driver.cpp
 * @brief   Implémentation du driver sonde carburant — Fortico.
 *
 * @details Chaîne de traitement :
 *
 *            AdcHal::readAvg()
 *              -> moyenne ADC en mV  (sur-échantillonnage dans HAL)
 *              -> conversion en volts
 *              -> validation plage physique
 *              -> filtre EMA
 *              -> stockage dans _emaVoltage / _lastVoltage
 *
 *          Le Driver ne contient aucune logique métier carburant.
 *          L'interprétation du niveau, des seuils et des alertes
 *          appartient au FuelManager.
 */
#include "drivers/fuel/fuel_driver.h"

#include "config/hardware_config.h"
#include "config/pin_config.h"

namespace fortico {
namespace drivers {

// ============================================================================
// Constantes locales
// ============================================================================


// ============================================================================
// Constructeur
// ============================================================================

FuelDriver::FuelDriver(hal::AdcHal& adcHal)
    : _adcHal(adcHal)
    , _initialized(false)
    , _healthy(false)
    , _active(false)
    , _consecutiveFailures(0U)
    , _lastVoltage(0.0F)
    , _emaVoltage(0.0F)
{
}

// ============================================================================
// IDriver — cycle de vie
// ============================================================================

constants::ErrorCode FuelDriver::init()
{
    if (_initialized) {
        return constants::ErrorCode::OK;
    }

    const bool halOk = _adcHal.init(
        pins::FUEL_ADC,
        hardware::ADC_ATTENUATION_DB);

    if (!halOk) {
        _healthy = false;
        return constants::ErrorCode::FUEL_SENSOR_FAILURE;
    }

    _initialized         = true;
    _active              = true;
    _consecutiveFailures = 0U;

    // -----------------------------------------------------------------
    // Première mesure pour amorcer l'EMA — évite le transitoire
    // au premier appel de readVoltage() ou poll().
    // -----------------------------------------------------------------
    const float rawAvgMv =
        _adcHal.readAvg(hardware::ADC_OVERSAMPLE_COUNT);
    const float initialVoltage = rawAvgMv / 1000.0F;

    if (initialVoltage < hardware::FUEL_MIN_VALID_VOLTAGE ||
        initialVoltage > hardware::FUEL_MAX_VALID_VOLTAGE) {
        // Hors plage au démarrage : driver dégradé mais initialisé.
        ++_consecutiveFailures;
        _healthy = false;
        return constants::ErrorCode::FUEL_SENSOR_FAILURE;
    }

    _emaVoltage  = initialVoltage;
    _lastVoltage = initialVoltage;
    _healthy     = true;

    return constants::ErrorCode::OK;
}

// ----------------------------------------------------------------------------

constants::ErrorCode FuelDriver::poll()
{
    if (!_initialized) {
        return constants::ErrorCode::NOT_INITIALIZED;
    }

    // Driver en veille volontaire (stop() appelé par PowerManager).
    if (!_active) {
        return constants::ErrorCode::OK;
    }

    // Sur-échantillonnage délégué au HAL.
    const float rawAvgMv =
        _adcHal.readAvg(hardware::ADC_OVERSAMPLE_COUNT);
    const float voltageV = rawAvgMv / 1000.0F;

    // Validation de la plage physique.
    if (voltageV < hardware::FUEL_MIN_VALID_VOLTAGE ||
        voltageV > hardware::FUEL_MAX_VALID_VOLTAGE) {

        if (_consecutiveFailures < hardware::DRIVER_MAX_CONSECUTIVE_FAILURES) {
            ++_consecutiveFailures;
        }
        if (_consecutiveFailures >= hardware::DRIVER_MAX_CONSECUTIVE_FAILURES) {
            _healthy = false;
        }
        return constants::ErrorCode::FUEL_SENSOR_FAILURE;
    }

    // Mesure valide — réinitialisation du compteur.
    _consecutiveFailures = 0U;

    // Filtre EMA : y[n] = alpha*x[n] + (1-alpha)*y[n-1]
    _emaVoltage =
        hardware::ADC_EMA_ALPHA * voltageV
        + (1.0F - hardware::ADC_EMA_ALPHA) * _emaVoltage;

    _lastVoltage = _emaVoltage;
    _healthy     = true;

    return constants::ErrorCode::OK;
}

// ----------------------------------------------------------------------------

bool FuelDriver::start()
{
    if (!_initialized) {
        return false;
    }

    _active = true;

    // Ne force pas _healthy=true si le capteur est toujours dégradé.
    if (_consecutiveFailures < hardware::DRIVER_MAX_CONSECUTIVE_FAILURES) {
        _healthy = true;
    }

    return true;
}

// ----------------------------------------------------------------------------

bool FuelDriver::stop()
{
    if (!_initialized) {
        return false;
    }

    _active = false;
    // Le driver conserve son état — pas de perte d'information.
    return true;
}

// ----------------------------------------------------------------------------

bool FuelDriver::isHealthy() const
{
    return _initialized  && _healthy;
}

// ----------------------------------------------------------------------------

bool FuelDriver::isInitialized() const
{
    return _initialized;
}

// ----------------------------------------------------------------------------

void FuelDriver::reset()
{
    _consecutiveFailures = 0U;
    _emaVoltage          = 0.0F;
    _lastVoltage         = 0.0F;

    if (!_initialized) {
        _healthy = false;
        _active  = false;
        return;
    }

    // Le HAL n'est pas réinitialisé — il est toujours correctement
    // configuré. Le prochain poll() reconstruira l'EMA proprement.
    _healthy = true;
}

// ============================================================================
// IFuelDriver — interface métier
// ============================================================================

bool FuelDriver::readVoltage(float& voltage)
{
    if (!_initialized || !_active || !_healthy) {
        return false;
    }

    voltage = _lastVoltage;
    return true;
}

// ----------------------------------------------------------------------------

bool FuelDriver::readRaw(float& raw)
{
    if (!_initialized || !_active || !_healthy) {
        return false;
    }

    // Lecture brute moyenne : HAL réalise le sur-échantillonnage,
    // mais aucun filtre EMA n'est appliqué ici.
    const float rawAvgMv =
        _adcHal.readAvg(hardware::ADC_OVERSAMPLE_COUNT);
    raw = rawAvgMv / 1000.0F;

    return true;
}

}  // namespace drivers
}  // namespace fortico