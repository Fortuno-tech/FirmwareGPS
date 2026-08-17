#pragma once

#include <cstdint>
#include <cstddef>

namespace fortico {
namespace interfaces {

class INvsDriver {
public:
    virtual ~INvsDriver() = default;

    virtual bool open(const char* namespaceName, bool readOnly) = 0;

    virtual void close() = 0;

    virtual bool putString(
        const char* key,
        const char* value) = 0;

    virtual bool getString(
        const char* key,
        char* buffer,
        std::size_t bufferSize) = 0;

    virtual bool putUInt32(
        const char* key,
        std::uint32_t value) = 0;

    virtual bool getUInt32(
        const char* key,
        std::uint32_t& value) = 0;

    virtual bool putBlob(
        const char* key,
        const void* data,
        std::size_t length) = 0;

    virtual bool getBlob(
        const char* key,
        void* data,
        std::size_t length) = 0;

    virtual bool remove(const char* key) = 0;

    virtual bool clear() = 0;
};

}  // namespace interfaces
}  // namespace fortico