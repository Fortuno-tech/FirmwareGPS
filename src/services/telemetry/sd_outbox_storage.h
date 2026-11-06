#pragma once

#include "services/telemetry/telemetry_outbox.h"

#if defined(ESP32_TARGET)
#include <SD.h>

namespace fortico {
namespace services {

class SdOutboxStorage final : public OutboxStorage {
public:
    explicit SdOutboxStorage(fs::SDFS& filesystem) : _filesystem(filesystem) {}
    

    bool createDirectory(const char* path) override;
    bool listFiles(const char* path,
                   std::vector<std::string>& names) override;
    bool exists(const char* path) override;
    bool readFile(const char* path,
                  std::vector<uint8_t>& contents) override;
    bool writeFile(const char* path, const uint8_t* contents,
                   size_t length) override;
    bool renameFile(const char* from, const char* to) override;
    bool removeFile(const char* path) override;
    bool availableBytes(size_t& bytes) const override;

private:
    fs::SDFS& _filesystem;
};

}  // namespace services
}  // namespace fortico
#endif
