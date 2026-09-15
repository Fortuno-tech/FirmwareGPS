#pragma once

#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "interfaces/invs_driver.h"

namespace fortico {
namespace test {

class MockNvsDriver : public interfaces::INvsDriver {
public:
    MockNvsDriver() : _initialized(true), _open(false), _readOnly(false) {}

    void setInitialized(bool val) { _initialized = val; }
    bool isInitialized() const override { return _initialized; }

    bool open(const char* namespaceName, bool readOnly) override {
        if (!_initialized) return false;
        _currentNamespace = namespaceName != nullptr ? namespaceName : "";
        _open = true;
        _readOnly = readOnly;
        return true;
    }

    void close() override {
        _open = false;
    }

    bool putString(const char* key, const char* value) override {
        if (!_open || _readOnly || key == nullptr || value == nullptr) return false;
        std::string fullKey = _currentNamespace + ":" + key;
        _stringStore[fullKey] = value;
        return true;
    }

    bool getString(const char* key, char* buffer, std::size_t bufferSize) override {
        if (!_open || key == nullptr || buffer == nullptr || bufferSize == 0) return false;
        std::string fullKey = _currentNamespace + ":" + key;
        auto it = _stringStore.find(fullKey);
        if (it == _stringStore.end()) return false;
        if (it->second.size() >= bufferSize) return false;
        std::strncpy(buffer, it->second.c_str(), bufferSize);
        return true;
    }

    bool putUInt32(const char* key, std::uint32_t value) override {
        if (!_open || _readOnly || key == nullptr) return false;
        std::string fullKey = _currentNamespace + ":" + key;
        _uintStore[fullKey] = value;
        return true;
    }

    bool getUInt32(const char* key, std::uint32_t& value) override {
        if (!_open || key == nullptr) return false;
        std::string fullKey = _currentNamespace + ":" + key;
        auto it = _uintStore.find(fullKey);
        if (it == _uintStore.end()) return false;
        value = it->second;
        return true;
    }

    bool putBlob(const char* key, const void* data, std::size_t length) override {
        if (!_open || _readOnly || key == nullptr || data == nullptr || length == 0) return false;
        std::string fullKey = _currentNamespace + ":" + key;
        const uint8_t* ptr = static_cast<const uint8_t*>(data);
        _blobStore[fullKey] = std::vector<uint8_t>(ptr, ptr + length);
        return true;
    }

    bool getBlob(const char* key, void* data, std::size_t length) override {
        if (!_open || key == nullptr || data == nullptr) return false;
        std::string fullKey = _currentNamespace + ":" + key;
        auto it = _blobStore.find(fullKey);
        if (it == _blobStore.end() || it->second.size() != length) return false;
        std::memcpy(data, it->second.data(), length);
        return true;
    }

    bool remove(const char* key) override {
        if (!_open || _readOnly || key == nullptr) return false;
        std::string fullKey = _currentNamespace + ":" + key;
        _stringStore.erase(fullKey);
        _uintStore.erase(fullKey);
        _blobStore.erase(fullKey);
        return true;
    }

    bool clear() override {
        if (!_open || _readOnly) return false;
        _stringStore.clear();
        _uintStore.clear();
        _blobStore.clear();
        return true;
    }

private:
    bool _initialized;
    bool _open;
    bool _readOnly;
    std::string _currentNamespace;
    std::map<std::string, std::string> _stringStore;
    std::map<std::string, uint32_t> _uintStore;
    std::map<std::string, std::vector<uint8_t>> _blobStore;
};

}  // namespace test
}  // namespace fortico

