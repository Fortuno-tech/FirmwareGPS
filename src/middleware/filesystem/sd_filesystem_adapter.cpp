#include "middleware/filesystem/sd_filesystem_adapter.h"

namespace fortico {
namespace middleware {

bool SdFileSystemAdapter::append(const char* path, const void* data,
                                 std::size_t length)
{
    return _driver.append(path, data, length);
}

bool SdFileSystemAdapter::read(const char* path, void* data, std::size_t length,
                               std::size_t& readLength)
{
    return _driver.read(path, data, length, readLength);
}

bool SdFileSystemAdapter::remove(const char* path)
{
    return _driver.remove(path);
}

}  // namespace middleware
}  // namespace fortico