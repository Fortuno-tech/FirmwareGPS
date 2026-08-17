/**
 * @file    nvs_driver.h
 * @brief   Driver NVS (Preferences ESP32) — Fortico.
 *
 * @details Encapsule NvsHal. Gère l'ouverture/fermeture de namespace,
 *          la tolérance aux pannes et le cycle de vie IDriver.
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "interfaces/idriver.h"
#include "interfaces/invs_driver.h"
#include "config/hardware_config.h"
#include "hal/nvs/nvs_hal.h"
#include "constants/errors.h"

namespace fortico {
namespace drivers {

class NvsDriver :
    public interfaces::IDriver,
    public interfaces::INvsDriver
{
public:
    explicit NvsDriver(hal::NvsHal& nvsHal);

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
    // INvsDriver — interface métier
    // -------------------------------------------------------------------------
    bool open(const char* namespaceName, bool readOnly) override;
    void close()                                         override;

    bool putString(const char* key, const char* value)               override;
    bool getString(const char* key, char* buffer, std::size_t size)  override;
    bool putUInt32(const char* key, std::uint32_t value)             override;
    bool getUInt32(const char* key, std::uint32_t& value)            override;
    bool putBlob(const char* key, const void* data, std::size_t len) override;
    bool getBlob(const char* key, void* data, std::size_t len)       override;
    bool remove(const char* key)                                     override;
    bool clear()                                                     override;

private:
    hal::NvsHal& _nvsHal;

    // État du driver
    bool _initialized;
    bool _healthy;
    bool _active;

    // État de la session NVS
    bool _namespaceOpen;
    char _currentNamespace[16];
    bool _readOnly;

    // Tolérance aux erreurs
    uint8_t _consecutiveFailures;
};

}  // namespace drivers
}  // namespace fortico