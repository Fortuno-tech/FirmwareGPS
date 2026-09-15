#include "drivers/sd/sd_driver.h"

#include "config/hardware_config.h"
#include "config/pin_config.h"

#ifdef ESP32_TARGET
#include <SD.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

namespace fortico {
namespace drivers {

// Vitesse de sécurité SPI pour la carte SD (4 MHz max pour éviter le bruit sur PCB/fils)
static constexpr uint32_t SAFE_SD_SPI_SPEED = 4000000U;

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
    , _remountAttempts(0U)
    , _lastRemountAttemptMs(0U)
    , _lastHealthCheckMs(0U) // Initialisation du timer
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

    // Montage SD en utilisant le SPIClass géré par SpiHal avec vitesse sécurisée.
    bool mounted = false;
    for (uint8_t attempt = 0U; attempt < 3U && !mounted; ++attempt) {
        mounted = SD.begin(pins::SD_CS, _spiHal.native(), SAFE_SD_SPI_SPEED);
        if (!mounted && attempt < 2U) {
            SD.end();
            vTaskDelay(pdMS_TO_TICKS(50U));
        }
    }
    if (!mounted) {
        _healthy = false;
        _mounted = false;
        return constants::ErrorCode::SD_NOT_MOUNTED;
    }

    _mounted = true;
    _lastHealthCheckMs = millis();
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

#ifdef ESP32_TARGET
    const uint32_t nowMs = static_cast<uint32_t>(millis());

    if (_mounted) {
        // ⚠️ CRITIQUE : N'interroger la carte SD physiquement qu'une fois toutes les 5 secondes
        // pour éviter le Card Hammering (CMD13 / Error 0x0d).
        if (nowMs - _lastHealthCheckMs >= 5000U) {
            _lastHealthCheckMs = nowMs;
            if (SD.cardType() == CARD_NONE) {
                _mounted = false;
                SD.end();
                _remountAttempts = 0U;
            } else {
                _consecutiveFailures = 0U;
                return constants::ErrorCode::OK;
            }
        } else {
            // Entre deux vérifications, on considère que la carte va bien
            _consecutiveFailures = 0U;
            return constants::ErrorCode::OK;
        }
    }

    // Card is not mounted — attempt periodic remount with back-off.
    const uint32_t cooldownMs = (_remountAttempts < 3U) ? 2000U : 10000U;
    if (nowMs - _lastRemountAttemptMs < cooldownMs) {
        return constants::ErrorCode::SD_NOT_MOUNTED;
    }
    _lastRemountAttemptMs = nowMs;
    ++_remountAttempts;

    SD.end();
    _spiHal.init(
        pins::SD_SCK,
        pins::SD_MOSI,
        pins::SD_MISO,
        pins::SD_CS);

    // Vitesse lente (2 MHz) pour les essais de reconnexion
    const uint32_t remountFreq = 2000000U;

    if (SD.begin(pins::SD_CS, _spiHal.native(), remountFreq)) {
        _mounted             = true;
        _healthy             = true;
        _consecutiveFailures = 0U;
        _remountAttempts     = 0U;
        _lastHealthCheckMs   = nowMs;
        return constants::ErrorCode::OK;
    }

    SD.end();
#endif

    if (_consecutiveFailures < hardware::DRIVER_MAX_CONSECUTIVE_FAILURES) {
        ++_consecutiveFailures;
    }
    if (_consecutiveFailures >= hardware::DRIVER_MAX_CONSECUTIVE_FAILURES) {
        _healthy = false;
    }
    return constants::ErrorCode::SD_NOT_MOUNTED;
}

// ----------------------------------------------------------------------------

bool SdDriver::start()
{
    if (!_initialized) {
        return false;
    }

#ifdef ESP32_TARGET
    if (!_mounted) {
        if (!SD.begin(pins::SD_CS, _spiHal.native(), SAFE_SD_SPI_SPEED)) {
            _healthy = false;
            return false;
        }
        _mounted = true;
        _lastHealthCheckMs = millis();
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

    _consecutiveFailures  = 0U;
    _remountAttempts      = 0U;
    _lastRemountAttemptMs = 0U;
    _lastHealthCheckMs    = 0U;

    if (!_initialized) {
        _healthy = false;
        return;
    }

#ifdef ESP32_TARGET
    _spiHal.init(
        pins::SD_SCK,
        pins::SD_MOSI,
        pins::SD_MISO,
        pins::SD_CS);

    if (SD.begin(pins::SD_CS, _spiHal.native(), SAFE_SD_SPI_SPEED)) {
        _mounted = true;
        _healthy = true;
        _active  = true;
        _lastHealthCheckMs = millis();
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