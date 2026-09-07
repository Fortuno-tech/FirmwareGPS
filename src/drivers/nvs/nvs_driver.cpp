
/*/
 * @file    nvs_driver.cpp
 * @brief   Implémentation du driver NVS — Fortico.
 *
 * @details Encapsule NvsHal et fournit :
 *            - gestion du namespace (session logique) ;
 *            - opérations CRUD typées ;
 *            - cycle de vie IDriver complet ;
 *            - surveillance de santé et tolérance aux pannes.
 *
 *          Ce Driver ne contient aucune politique métier.
 *          Il n'inclut jamais Preferences.h directement.
 */
#include "drivers/nvs/nvs_driver.h"

#include <cstring>

namespace fortico {
namespace drivers {

// ============================================================================
// Constantes locales
// ============================================================================

namespace {

/// Taille d'un uint32_t en octets
constexpr std::size_t kUInt32Size = sizeof(std::uint32_t);

/// Longueur maximale d'un namespace NVS ESP32 (15 chars utiles)
constexpr std::size_t kMaxNamespaceLength = 15U;

/// Namespace utilisé pour le test de santé à l'init
constexpr const char* kProbeNamespace = "_drv_probe";

/// Clé utilisée pour le test de santé à l'init
constexpr const char* kProbeKey = "probe";

/// Valeur sentinelle pour le test de santé
constexpr uint32_t kProbeValue = 0xDEADBEEFUL;

}  // namespace

// ============================================================================
// Constructeur
// ============================================================================

NvsDriver::NvsDriver(hal::NvsHal& nvsHal)
    : _nvsHal(nvsHal)
    , _initialized(false)
    , _healthy(false)
    , _active(false)
    , _namespaceOpen(false)
    , _currentNamespace{}
    , _readOnly(false)
    , _consecutiveFailures(0U)
{
}

// ============================================================================
// IDriver — cycle de vie
// ============================================================================

constants::ErrorCode NvsDriver::init()
{
    if (_initialized) {
        return constants::ErrorCode::OK;
    }

    // -----------------------------------------------------------------
    // Test de santé : écriture + lecture + vérification + nettoyage.
    // NvsHal est stateless, donc ce test valide l'accès réel à la NVS.
    // -----------------------------------------------------------------
    const uint8_t* probeData =
        reinterpret_cast<const uint8_t*>(&kProbeValue);

    const bool writeOk = _nvsHal.set(
        kProbeNamespace,
        kProbeKey,
        probeData,
        kUInt32Size);

    if (!writeOk) {
        _healthy = false;
        return constants::ErrorCode::NVS_WRITE_FAILED;
    }

    uint32_t    readBack   = 0U;
    std::size_t readLength = kUInt32Size;

    const bool readOk = _nvsHal.read(
        kProbeNamespace,
        kProbeKey,
        reinterpret_cast<uint8_t*>(&readBack),
        &readLength);

    // Nettoyage systématique de la clé de probe.
    _nvsHal.erase(kProbeNamespace, kProbeKey);

    if (!readOk ||
        readLength != kUInt32Size ||
        readBack   != kProbeValue) {
        _healthy = false;
        return constants::ErrorCode::NVS_READ_FAILED;
    }

    _initialized         = true;
    _active              = true;
    _healthy             = true;
    _consecutiveFailures = 0U;

    return constants::ErrorCode::OK;
}

// ----------------------------------------------------------------------------

constants::ErrorCode NvsDriver::poll()
{
    if (!_initialized) {
        return constants::ErrorCode::NOT_INITIALIZED;
    }

    // NVS ne nécessite pas de polling matériel.
    // On surveille uniquement le compteur d'échecs accumulé
    // par les opérations CRUD sans forcer _healthy=true.
    if (_consecutiveFailures >= hardware::DRIVER_MAX_CONSECUTIVE_FAILURES) {
        _healthy = false;
        return constants::ErrorCode::NVS_READ_FAILED;
    }

    return constants::ErrorCode::OK;
}

// ----------------------------------------------------------------------------

bool NvsDriver::start()
{
    if (!_initialized) {
        return false;
    }

    _active = true;

    // Ne force pas _healthy si le driver est encore dégradé.
    if (_consecutiveFailures < hardware::DRIVER_MAX_CONSECUTIVE_FAILURES) {
        _healthy = true;
    }

    return true;
}

// ----------------------------------------------------------------------------

bool NvsDriver::stop()
{
    if (!_initialized) {
        return false;
    }

    if (_namespaceOpen) {
        close();
    }

    _active = false;
    return true;
}

// ----------------------------------------------------------------------------

bool NvsDriver::isHealthy() const
{
    return _initialized && _healthy;
}
// ----------------------------------------------------------------------------

bool NvsDriver::isInitialized() const
{
    return _initialized;
}

// ----------------------------------------------------------------------------

void NvsDriver::reset()
{
    if (_namespaceOpen) {
        close();
    }

    _consecutiveFailures = 0U;
    _healthy = _initialized;
}

// ============================================================================
// INvsDriver — gestion du namespace (session)
// ============================================================================

bool NvsDriver::open(const char* namespaceName, bool readOnly)
{
    if (!_initialized || !_active) {
        return false;
    }

    if (namespaceName == nullptr || namespaceName[0] == '\0') {
        return false;
    }

    if (std::strlen(namespaceName) > kMaxNamespaceLength) {
        return false;
    }

    // Fermeture propre de la session précédente si encore ouverte.
    if (_namespaceOpen) {
        close();
    }

    std::strncpy(
        _currentNamespace,
        namespaceName,
        sizeof(_currentNamespace) - 1U);
    _currentNamespace[sizeof(_currentNamespace) - 1U] = '\0';

    _readOnly      = readOnly;
    _namespaceOpen = true;

    return true;
}

// ----------------------------------------------------------------------------

void NvsDriver::close()
{
    _namespaceOpen       = false;
    _currentNamespace[0] = '\0';
    _readOnly            = false;
}

// ============================================================================
// INvsDriver — opérations CRUD
// ============================================================================

bool NvsDriver::putString(const char* key, const char* value)
{
    if (!_namespaceOpen || _readOnly ||
        key == nullptr || value == nullptr) {
        return false;
    }

    const std::size_t length = std::strlen(value) + 1U;  // inclut '\0'

    const bool ok = _nvsHal.set(
        _currentNamespace,
        key,
        reinterpret_cast<const uint8_t*>(value),
        length);

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
}

// ----------------------------------------------------------------------------

bool NvsDriver::getString(
    const char* key,
    char*       buffer,
    std::size_t bufferSize)
{
    if (!_namespaceOpen ||
        key == nullptr || buffer == nullptr || bufferSize == 0U) {
        return false;
    }

    std::size_t length = bufferSize;

    const bool ok = _nvsHal.read(
        _currentNamespace,
        key,
        reinterpret_cast<uint8_t*>(buffer),
        &length);

    if (!ok) {
        if (_consecutiveFailures < hardware::DRIVER_MAX_CONSECUTIVE_FAILURES) {
            ++_consecutiveFailures;
        }
        if (_consecutiveFailures >= hardware::DRIVER_MAX_CONSECUTIVE_FAILURES) {
            _healthy = false;
        }
        buffer[0] = '\0';
        return false;
    }

    // Garantit la terminaison même si length == bufferSize.
    buffer[bufferSize - 1U] = '\0';
    _consecutiveFailures = 0U;
    return true;
}

// ----------------------------------------------------------------------------

bool NvsDriver::putUInt32(const char* key, uint32_t value)
{
    if (!_namespaceOpen || _readOnly || key == nullptr) {
        return false;
    }

    const bool ok = _nvsHal.set(
        _currentNamespace,
        key,
        reinterpret_cast<const uint8_t*>(&value),
        kUInt32Size);

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
}

// ----------------------------------------------------------------------------

bool NvsDriver::getUInt32(const char* key, uint32_t& value)
{
    if (!_namespaceOpen || key == nullptr) {
        return false;
    }

    std::size_t length = kUInt32Size;

    const bool ok = _nvsHal.read(
        _currentNamespace,
        key,
        reinterpret_cast<uint8_t*>(&value),
        &length);

    if (!ok || length != kUInt32Size) {
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
}

// ----------------------------------------------------------------------------

bool NvsDriver::putBlob(
    const char* key,
    const void* data,
    std::size_t length)
{
    if (!_namespaceOpen || _readOnly ||
        key == nullptr || data == nullptr || length == 0U) {
        return false;
    }

    const bool ok = _nvsHal.set(
        _currentNamespace,
        key,
        reinterpret_cast<const uint8_t*>(data),
        length);

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
}

// ----------------------------------------------------------------------------

bool NvsDriver::getBlob(
    const char* key,
    void*       data,
    std::size_t length)
{
    if (!_namespaceOpen ||
        key == nullptr || data == nullptr || length == 0U) {
        return false;
    }

    std::size_t readLength = length;

    const bool ok = _nvsHal.read(
        _currentNamespace,
        key,
        reinterpret_cast<uint8_t*>(data),
        &readLength);

    if (!ok || readLength != length) {
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
}

// ----------------------------------------------------------------------------

bool NvsDriver::remove(const char* key)
{
    if (!_namespaceOpen || _readOnly || key == nullptr) {
        return false;
    }

    const bool ok = _nvsHal.erase(_currentNamespace, key);

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
}

// ----------------------------------------------------------------------------


bool NvsDriver::clear()
{
    if (!_namespaceOpen || _readOnly) {
        return false;
    }

    const bool ok = _nvsHal.clear(_currentNamespace);

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
}
} 
}  // namespace fortico