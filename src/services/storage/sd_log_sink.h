#pragma once

#include "core/logger/logger.h"
#include "interfaces/isd_driver.h"

namespace fortico {
namespace services {

/// Persiste les lignes du Logger dans un fichier texte à la racine de la SD.
class SdLogSink final : public core::ILogSink {
public:
    explicit SdLogSink(interfaces::ISdDriver& driver) : _driver(driver) {}

    void write(const char* line) override;

private:
    interfaces::ISdDriver& _driver;
};

}  // namespace services
}  // namespace fortico
