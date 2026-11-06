#pragma once
#include <cstddef>
#include <cstdarg>
#include <cstdio>
namespace fortico { namespace middleware {
class JsonWriter {
public:
    JsonWriter(char* buffer, std::size_t capacity) : _buffer(buffer), _capacity(capacity), _length(0U) { if (_buffer && _capacity) _buffer[0] = '\0'; }
    bool format(const char* format, ...) {
        if (!_buffer || !_capacity || !format) return false;
        va_list args; va_start(args, format); const int written = std::vsnprintf(_buffer, _capacity, format, args); va_end(args);
        _length = written > 0 ? static_cast<std::size_t>(written) : 0U;
        return written >= 0 && _length < _capacity;
    }
    const char* data() const { return _buffer; }
    std::size_t size() const { return _length; }
private: char* _buffer; std::size_t _capacity; std::size_t _length;
};
} }