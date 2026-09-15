#pragma once

#include <cstddef>
#include <cstdint>

namespace fortico {
namespace interfaces {

class ISdDriver {
public:
    virtual ~ISdDriver() = default;

    virtual bool exists(const char* path) = 0;

    virtual bool createDirectory(const char* path) = 0;

    virtual bool append(
        const char* path,
        const void* data,
        std::size_t length) = 0;

    virtual bool read(
        const char* path,
        void* buffer,
        std::size_t bufferSize,
        std::size_t& bytesRead) = 0;

    virtual bool write(
        const char* path,
        const void* data,
        std::size_t length) = 0;

    virtual bool remove(const char* path) = 0;

    virtual bool flush() = 0;

    virtual std::uint64_t fileSize(const char* path) = 0;
};

}  // namespace interfaces
}  // namespace fortico