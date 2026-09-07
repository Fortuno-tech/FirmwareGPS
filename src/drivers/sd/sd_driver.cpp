/**
 * @file    sd_driver.cpp
 * @brief   Implémentation du driver carte SD — Fortico.
 *
 * @details Utilise SpiHal::native() pour passer le bon objet SPIClass
 *          à SD.begin(), garantissant que SD.h utilise exactement le
 *          même bus SPI que celui géré par le HAL.
 *
 *          Le Driver ouvre et ferme les fichiers à chaque opération :
 *          il n'y a pas de File persistant — le flush est implicite.
 *          La politique de buffering appartient au StorageManager.
 */
#include "drivers/sd/sd_driver.h"

#include "config/hardware_config.h"
#include "config/pin_config.h"

#ifdef ESP32_TARGET
#include <SD.h>
#endif

namespace fortico {
namespace drivers {

// ============================================================================
// Constantes locales
// ============================================================================

  // namespace

// ============================================================================
// Constructeur
// ============================================================================

SdDriver::SdDriver(hal::SpiHal& spiHal)
    : _spiHal(spiHal)
    , _initialized(false)
    , _healthy(false)
    , _active(false)
    , _mounted(false)
    , _consecutiveFailures(0U)
{
}

// ============================================================================
// IDriver — cycle de vie
// ============================================================================

constants::ErrorCode SdDriver::init()
{
    if (_initialized) {
        return constants::ErrorCode::OK;
    }

#ifdef ESP32_TARGET
    // Initialisation du bus SPI via HAL.
    const bool spiOk = _spiHal.init(
        pins::SD_SCK,
        pins::SD_MOSI,
        pins::SD_MISO,
        pins::SD_CS);

    if (!spiOk) {
        _healthy = false;
        return constants::ErrorCode::SD_FAILURE;
    }

    // Montage SD en utilisant le SPIClass géré par SpiHal.
    // SD.begin() avec SPIClass& garantit l'absence de conflit de bus.
    if (!SD.begin(pins::SD_CS, _spiHal.native(), hardware::SPI_FREQUENCY_HZ)) {
        _healthy = false;
        _mounted = false;
        return constants::ErrorCode::SD_NOT_MOUNTED;
    }

    _mounted = true;
#else
    return constants::ErrorCode::SD_FAILURE;
#endif

    _initialized         = true;
    _active              = true;
    _healthy             = true;
    _consecutiveFailures = 0U;

    return constants::ErrorCode::OK;
}

// ----------------------------------------------------------------------------

constants::ErrorCode SdDriver::poll()
{
    if (!_initialized) {
        return constants::ErrorCode::NOT_INITIALIZED;
    }

    if (!_active) {
        return constants::ErrorCode::OK;
    }

    if (!_mounted) {
        if (_consecutiveFailures < hardware::DRIVER_MAX_CONSECUTIVE_FAILURES) {
            ++_consecutiveFailures;
        }
        if (_consecutiveFailures >= hardware::DRIVER_MAX_CONSECUTIVE_FAILURES) {
            _healthy = false;
        }
        return constants::ErrorCode::SD_NOT_MOUNTED;
    }

    return constants::ErrorCode::OK;
}

// ----------------------------------------------------------------------------

bool SdDriver::start()
{
    if (!_initialized) {
        return false;
    }

#ifdef ESP32_TARGET
    if (!_mounted) {
        if (!SD.begin(pins::SD_CS, _spiHal.native(), hardware::SPI_FREQUENCY_HZ)) {
            _healthy = false;
            return false;
        }
        _mounted = true;
    }
#endif

    _active              = true;
    _healthy             = true;
    _consecutiveFailures = 0U;
    return true;
}

// ----------------------------------------------------------------------------

bool SdDriver::stop()
{
    if (!_initialized) {
        return false;
    }

#ifdef ESP32_TARGET
    if (_mounted) {
        SD.end();
    }
    _mounted = false;
#endif

    _active = false;
    return true;
}

// ----------------------------------------------------------------------------

bool SdDriver::isHealthy() const
{
    return _initialized && _mounted && _healthy;
}

// ----------------------------------------------------------------------------

bool SdDriver::isInitialized() const
{
    return _initialized;
}

// ----------------------------------------------------------------------------

void SdDriver::reset()
{
#ifdef ESP32_TARGET
    if (_mounted) {
        SD.end();
    }
    _mounted = false;
#endif

    _consecutiveFailures = 0U;

    if (!_initialized) {
        _healthy = false;
        return;
    }

#ifdef ESP32_TARGET
    if (SD.begin(pins::SD_CS, _spiHal.native(), hardware::SPI_FREQUENCY_HZ)) {
        _mounted = true;
        _healthy = true;
        _active  = true;
    } else {
        _mounted = false;
        _healthy = false;
    }
#else
    _healthy = false;
#endif
}

// ============================================================================
// ISdDriver — opérations fichier
// ============================================================================

bool SdDriver::exists(const char* path)
{
    if (!_initialized || !_active || !_mounted || path == nullptr) {
        return false;
    }

#ifdef ESP32_TARGET
    return SD.exists(path);
#else
    return false;
#endif
}

// ----------------------------------------------------------------------------

bool SdDriver::createDirectory(const char* path)
{
    if (!_initialized || !_active || !_mounted || path == nullptr) {
        return false;
    }

#ifdef ESP32_TARGET
    const bool ok = SD.mkdir(path);
    if (!ok) {
        if (_consecutiveFailures < hardware::DRIVER_MAX_CONSECUTIVE_FAILURES) {
            ++_consecutiveFailures;
        }
        if (_consecutiveFailures >= hardware::DRIVER_MAX_CONSECUTIVE_FAILURES) {
            _healthy = false;
        }
        return false;
    }
    _consecutiveFailures = 0U;
    return true;
#else
    return false;
#endif
}

// ----------------------------------------------------------------------------

bool SdDriver::append(
    const char* path,
    const void* data,
    std::size_t length)
{
    if (!_initialized || !_active || !_mounted ||
        path == nullptr || data == nullptr || length == 0U) {
        return false;
    }

#ifdef ESP32_TARGET
    File file = SD.open(path, FILE_APPEND);
    if (!file) {
        if (_consecutiveFailures < hardware::DRIVER_MAX_CONSECUTIVE_FAILURES) {
            ++_consecutiveFailures;
        }
        if (_consecutiveFailures >= hardware::DRIVER_MAX_CONSECUTIVE_FAILURES) {
            _healthy = false;
        }
        return false;
    }

    const std::size_t written =
        file.write(reinterpret_cast<const uint8_t*>(data), length);
    file.close();

    if (written != length) {
        if (_consecutiveFailures < hardware::DRIVER_MAX_CONSECUTIVE_FAILURES) {
            ++_consecutiveFailures;
        }
        if (_consecutiveFailures >= hardware::DRIVER_MAX_CONSECUTIVE_FAILURES) {
            _healthy = false;
        }
        return false;
    }

    _consecutiveFailures = 0U;
    return true;
#else
    return false;
#endif
}

// ----------------------------------------------------------------------------

bool SdDriver::read(
    const char* path,
    void*       buffer,
    std::size_t bufferSize,
    std::size_t& bytesRead)
{
    bytesRead = 0U;

    if (!_initialized || !_active || !_mounted ||
        path == nullptr || buffer == nullptr || bufferSize == 0U) {
        return false;
    }

#ifdef ESP32_TARGET
    File file = SD.open(path, FILE_READ);
    if (!file) {
        if (_consecutiveFailures < hardware::DRIVER_MAX_CONSECUTIVE_FAILURES) {
            ++_consecutiveFailures;
        }
        if (_consecutiveFailures >= hardware::DRIVER_MAX_CONSECUTIVE_FAILURES) {
            _healthy = false;
        }
        return false;
    }

    bytesRead = file.read(
        reinterpret_cast<uint8_t*>(buffer),
        bufferSize);
    file.close();

    _consecutiveFailures = 0U;
    return true;
#else
    return false;
#endif
}

// ----------------------------------------------------------------------------

bool SdDriver::write(
    const char* path,
    const void* data,
    std::size_t length)
{
    if (!_initialized || !_active || !_mounted ||
        path == nullptr || data == nullptr || length == 0U) {
        return false;
    }

#ifdef ESP32_TARGET
    File file = SD.open(path, FILE_WRITE);
    if (!file) {
        if (_consecutiveFailures < hardware::DRIVER_MAX_CONSECUTIVE_FAILURES) {
            ++_consecutiveFailures;
        }
        if (_consecutiveFailures >= hardware::DRIVER_MAX_CONSECUTIVE_FAILURES) {
            _healthy = false;
        }
        return false;
    }

    const std::size_t written =
        file.write(reinterpret_cast<const uint8_t*>(data), length);
    file.close();

    if (written != length) {
        if (_consecutiveFailures < hardware::DRIVER_MAX_CONSECUTIVE_FAILURES) {
            ++_consecutiveFailures;
        }
        if (_consecutiveFailures >= hardware::DRIVER_MAX_CONSECUTIVE_FAILURES) {
            _healthy = false;
        }
        return false;
    }

    _consecutiveFailures = 0U;
    return true;
#else
    return false;
#endif
}

// ----------------------------------------------------------------------------

bool SdDriver::remove(const char* path)
{
    if (!_initialized || !_active || !_mounted || path == nullptr) {
        return false;
    }

#ifdef ESP32_TARGET
    const bool ok = SD.remove(path);
    if (!ok) {
        if (_consecutiveFailures < hardware::DRIVER_MAX_CONSECUTIVE_FAILURES) {
            ++_consecutiveFailures;
        }
        if (_consecutiveFailures >= hardware::DRIVER_MAX_CONSECUTIVE_FAILURES) {
            _healthy = false;
        }
        return false;
    }
    _consecutiveFailures = 0U;
    return true;
#else
    return false;
#endif
}

// ----------------------------------------------------------------------------

bool SdDriver::flush()
{
    // Le Driver ouvre et ferme chaque fichier à chaque opération.
    // Le flush est donc implicite (file.close() = flush + fermeture).
    // Cette méthode satisfait ISdDriver et permet au StorageManager
    // de déclencher un flush explicite si un File persistant est
    // introduit dans une future itération.
    return _initialized && _active && _mounted;
}

// ----------------------------------------------------------------------------

std::uint64_t SdDriver::fileSize(const char* path)
{
    if (!_initialized || !_active || !_mounted || path == nullptr) {
        return 0ULL;
    }

#ifdef ESP32_TARGET
    File file = SD.open(path, FILE_READ);
    if (!file) {
        return 0ULL;
    }
    const std::uint64_t size =
        static_cast<std::uint64_t>(file.size());
    file.close();
    return size;
#else
    return 0ULL;
#endif
}

}  // namespace drivers
}  // namespace fortico