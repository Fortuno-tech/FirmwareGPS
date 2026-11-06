#pragma once

#include "drivers/sd/sd_driver.h"
#include "middleware/filesystem/filesystem.h"

namespace fortico {
namespace middleware {

class SdFileSystemAdapter final : public FileSystem {
public:
    explicit SdFileSystemAdapter(drivers::SdDriver& driver) : _driver(driver) {}

    bool append(const char* path, const void* data, std::size_t length) override;
    bool read(const char* path, void* data, std::size_t length,
              std::size_t& readLength) override;
    bool remove(const char* path) override;

private:
    drivers::SdDriver& _driver;
};

}  // namespace middleware
}  // namespace fortico