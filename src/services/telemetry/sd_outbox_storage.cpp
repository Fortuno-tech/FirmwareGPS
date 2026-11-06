#include "services/telemetry/sd_outbox_storage.h"

#if defined(ESP32_TARGET)

#include <cstring>
#include <limits>

namespace fortico {
namespace services {

bool SdOutboxStorage::createDirectory(const char* path)
{
    if (_filesystem.exists(path)) {
        File existing = _filesystem.open(path, FILE_READ);
        const bool directory = existing && existing.isDirectory();
        existing.close();
        return directory;
    }
    return _filesystem.mkdir(path);
}

bool SdOutboxStorage::listFiles(const char* path,
                                std::vector<std::string>& names)
{
    names.clear();
    File directory = _filesystem.open(path, FILE_READ);
    if (!directory || !directory.isDirectory()) {
        directory.close();
        return false;
    }

    File entry = directory.openNextFile();
    while (entry) {
        if (!entry.isDirectory()) {
            const char* name = entry.name();
            if (name != nullptr) {
                const char* basename = std::strrchr(name, '/');
                names.emplace_back(basename == nullptr ? name : basename + 1);
            }
        }
        entry.close();
        entry = directory.openNextFile();
    }
    directory.close();
    return true;
}

bool SdOutboxStorage::exists(const char* path)
{
    return _filesystem.exists(path);
}

bool SdOutboxStorage::readFile(const char* path,
                               std::vector<uint8_t>& contents)
{
    File file = _filesystem.open(path, FILE_READ);
    if (!file || file.isDirectory()) {
        file.close();
        return false;
    }
    const size_t fileSize = file.size();
    contents.resize(fileSize);
    const size_t readLength = file.read(contents.data(), fileSize);
    file.close();
    return readLength == fileSize;
}

bool SdOutboxStorage::writeFile(const char* path, const uint8_t* contents,
                                size_t length)
{
    if (_filesystem.exists(path) && !_filesystem.remove(path)) {
        return false;
    }
    File file = _filesystem.open(path, FILE_WRITE);
    if (!file) {
        return false;
    }
    const size_t written = file.write(contents, length);
    file.flush();
    const bool success = written == length;
    file.close();
    return success;
}

bool SdOutboxStorage::renameFile(const char* from, const char* to)
{
    return _filesystem.rename(from, to);
}

bool SdOutboxStorage::removeFile(const char* path)
{
    return !_filesystem.exists(path) || _filesystem.remove(path);
}

bool SdOutboxStorage::availableBytes(size_t& bytes) const
{
    const uint64_t total = _filesystem.totalBytes();
    const uint64_t used = _filesystem.usedBytes();
    if (total == 0U || used > total) {
        return false;
    }
    const uint64_t available = total - used;
    bytes = available > std::numeric_limits<size_t>::max()
                ? std::numeric_limits<size_t>::max()
                : static_cast<size_t>(available);
    return true;
}

}  // namespace services
}  // namespace fortico

#endif
