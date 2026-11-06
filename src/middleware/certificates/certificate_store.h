#pragma once
#include <cstddef>
namespace fortico { namespace middleware {
class CertificateStore {
public:
    CertificateStore(const char* ca, const char* client = nullptr, const char* key = nullptr)
        : _ca(ca), _client(client), _key(key) {}
    bool valid() const { return _ca != nullptr && _ca[0] != '\0'; }
    const char* ca() const { return _ca; }
    const char* client() const { return _client; }
    const char* privateKey() const { return _key; }
private:
    const char* _ca; const char* _client; const char* _key;
};
} }