#pragma once
#include <cstddef>
namespace fortico { namespace middleware {
class FileSystem {
public:
    virtual ~FileSystem() = default;
    virtual bool append(const char* path, const void* data, std::size_t length) = 0;
    virtual bool read(const char* path, void* data, std::size_t length, std::size_t& readLength) = 0;
    virtual bool remove(const char* path) = 0;
};
} }