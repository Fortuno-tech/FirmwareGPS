#pragma once

#include <cstddef>
#include <cstdint>

#include "interfaces/idriver.h"
#include "interfaces/isd_driver.h"
#include "hal/spi/spi_hal.h"
#include "constants/errors.h"

namespace fortico {
namespace drivers {

class SdDriver :
    public interfaces::IDriver,
    public interfaces::ISdDriver
{
public:
    explicit SdDriver(hal::SpiHal& spiHal);

    // IDriver — cycle de vie
    constants::ErrorCode init()          override;
    constants::ErrorCode poll()          override;
    bool                 start()         override;
    bool                 stop()          override;
    bool                 isHealthy()     const override;
    bool                 isInitialized() const override;
    void                 reset()         override;

    // ISdDriver — opérations fichier
    bool          exists(const char* path)                                        override;
    bool          createDirectory(const char* path)                               override;
    bool          append(const char* path, const void* data, std::size_t length)  override;
    bool          read(const char* path, void* buffer,
                       std::size_t bufferSize, std::size_t& bytesRead)            override;
    bool          write(const char* path, const void* data, std::size_t length)   override;
    bool          remove(const char* path)                                        override;
    bool          flush()                                                         override;
    std::uint64_t fileSize(const char* path)                                      override;

private:
    hal::SpiHal& _spiHal;

    bool     _initialized;
    bool     _healthy;
    bool     _active;
    bool     _mounted;

    uint8_t  _consecutiveFailures;
    uint8_t  _remountAttempts;
    uint32_t _lastRemountAttemptMs;
    uint32_t _lastHealthCheckMs; // 👈 AJOUT : Timer pour espacer les tests d'état matériel
};

}  // namespace drivers
}  // namespace fortico