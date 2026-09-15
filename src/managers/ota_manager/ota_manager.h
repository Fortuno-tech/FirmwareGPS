#pragma once

#include <cstddef>
#include <cstdint>

#include "constants/errors.h"
#include "interfaces/imanager.h"

#ifdef ESP32_TARGET
#include <sodium.h>
#endif

namespace fortico {
namespace managers {

class IOtaBackend {
public:
	virtual ~IOtaBackend() = default;
	virtual bool begin(std::size_t imageSize) = 0;
	virtual bool write(const uint8_t* data, std::size_t length) = 0;
	virtual bool finish() = 0;
	virtual void abort() = 0;
};

class IOtaSignatureVerifier {
public:
	virtual ~IOtaSignatureVerifier() = default;
	virtual bool begin(std::size_t imageSize) = 0;
	virtual bool update(const uint8_t* data, std::size_t length) = 0;
	virtual bool verify(const uint8_t* signature, std::size_t length) = 0;
	virtual void reset() = 0;
};

class Ed25519OtaVerifier final : public IOtaSignatureVerifier {
public:
	static constexpr std::size_t PublicKeySize = 32U;
	static constexpr std::size_t SignatureSize = 64U;

	explicit Ed25519OtaVerifier(const uint8_t* publicKey);
	bool begin(std::size_t imageSize) override;
	bool update(const uint8_t* data, std::size_t length) override;
	bool verify(const uint8_t* signature, std::size_t length) override;
	void reset() override;

private:
	uint8_t _publicKey[PublicKeySize];
	bool _configured;
	bool _active;
#ifdef ESP32_TARGET
	crypto_sign_ed25519ph_state _state;
#endif
};

class EspOtaBackend final : public IOtaBackend {
public:
	bool begin(std::size_t imageSize) override;
	bool write(const uint8_t* data, std::size_t length) override;
	bool finish() override;
	void abort() override;
};

class OtaManager : public interfaces::IManager {
public:
	OtaManager(IOtaBackend& backend, IOtaSignatureVerifier& verifier);
	void init() override;
	void update() override;
	void process() override;
	void reset() override;
	bool begin(std::size_t imageSize);
	bool write(const uint8_t* data, std::size_t length);
	bool finish(const uint8_t* signature, std::size_t signatureLength);
	bool inProgress() const;
	std::size_t bytesWritten() const;

private:
	IOtaBackend& _backend;
	IOtaSignatureVerifier& _verifier;
	bool _initialized;
	bool _inProgress;
	std::size_t _expectedSize;
	std::size_t _bytesWritten;
};

}  // namespace managers
}  // namespace fortico
