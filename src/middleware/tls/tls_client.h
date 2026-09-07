#pragma once
#include "middleware/certificates/certificate_store.h"
namespace fortico { namespace middleware {
class TlsClient {
public:
    bool configure(const CertificateStore& certificates) { _certificates = &certificates; _configured = certificates.valid(); return _configured; }
    bool isConfigured() const { return _configured; }
    const CertificateStore* certificates() const { return _certificates; }
private: bool _configured = false;
    const CertificateStore* _certificates = nullptr;
};
} }