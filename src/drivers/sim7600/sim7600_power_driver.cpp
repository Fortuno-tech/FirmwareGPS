#include "drivers/sim7600/sim7600_power_driver.h"

#include "config/hardware_config.h"
#include "config/pin_config.h"

namespace fortico {
namespace drivers {
Sim7600PowerDriver::Sim7600PowerDriver(hal::GpioHal& gpioHal,
                                       hal::DelayHal& delayHal)
	: _gpioHal(gpioHal), _delayHal(delayHal), _initialized(false), _healthy(false), _active(false),
      _powerOnIssued(false)
{
}

constants::ErrorCode Sim7600PowerDriver::init()
{
    if (_initialized) return constants::ErrorCode::OK;
    if (!_gpioHal.init(pins::SIM7600_PWRKEY, hal::GpioMode::Output)) {
        return constants::ErrorCode::NETWORK_FAILURE;
    }
    _gpioHal.write(pins::SIM7600_PWRKEY, hardware::SIM7600_PWRKEY_IDLE_LEVEL);
    _initialized = true;
    _healthy = true;
    _active = true;
    return constants::ErrorCode::OK;
}

constants::ErrorCode Sim7600PowerDriver::poll()
{
    return _initialized && _healthy ? constants::ErrorCode::OK
                                    : constants::ErrorCode::NOT_INITIALIZED;
}

bool Sim7600PowerDriver::start()
{
    if (!_initialized) return false;
    _gpioHal.write(pins::SIM7600_PWRKEY, hardware::SIM7600_PWRKEY_IDLE_LEVEL);
    _active = true;
    _healthy = true;
    return true;
}

bool Sim7600PowerDriver::stop()
{
    if (!_initialized) return false;
    _gpioHal.write(pins::SIM7600_PWRKEY, hardware::SIM7600_PWRKEY_IDLE_LEVEL);
    _active = false;
    return true;
}

bool Sim7600PowerDriver::isHealthy() const
{
    return _initialized && _active && _healthy;
}

bool Sim7600PowerDriver::isInitialized() const { return _initialized; }

void Sim7600PowerDriver::reset()
{
    if (!_initialized) {
        _healthy = false;
        _active = false;
        return;
    }
    _gpioHal.write(pins::SIM7600_PWRKEY, hardware::SIM7600_PWRKEY_IDLE_LEVEL);
    _healthy = true;
    _active = true;
    _powerOnIssued = false;
}

bool Sim7600PowerDriver::powerOn()
{
    if (!isHealthy()) return false;
    if (_powerOnIssued) return true;

    _gpioHal.write(pins::SIM7600_PWRKEY, hardware::SIM7600_PWRKEY_IDLE_LEVEL);
    _delayHal.waitMs(hardware::SIM7600_PWRKEY_SETTLE_MS);
    _gpioHal.write(pins::SIM7600_PWRKEY,
                   !hardware::SIM7600_PWRKEY_IDLE_LEVEL);
    _delayHal.waitMs(hardware::SIM7600_PWRKEY_PULSE_MS);
    _gpioHal.write(pins::SIM7600_PWRKEY, hardware::SIM7600_PWRKEY_IDLE_LEVEL);
    _delayHal.waitMs(hardware::SIM7600_BOOT_DELAY_MS);

    _powerOnIssued = true;
    return true;
}

}  // namespace drivers
}  // namespace fortico
