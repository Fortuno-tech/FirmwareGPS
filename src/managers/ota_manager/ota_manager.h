#pragma once

#include <cstddef>
#include <cstdint>

#include "constants/errors.h"
#include "interfaces/imanager.h"

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

class EspOtaBackend final : public IOtaBackend {
public:
	bool begin(std::size_t imageSize) override;
	bool write(const uint8_t* data, std::size_t length) override;
	bool finish() override;
	void abort() override;
};

class OtaManager : public interfaces::IManager {
public:
	explicit OtaManager(IOtaBackend& backend);
	void init() override;
	void update() override;
	void process() override;
	void reset() override;
	bool begin(std::size_t imageSize);
	bool write(const uint8_t* data, std::size_t length);
	bool finish();
	bool inProgress() const;
	std::size_t bytesWritten() const;

private:
	IOtaBackend& _backend;
	bool _initialized;
	bool _inProgress;
	std::size_t _expectedSize;
	std::size_t _bytesWritten;
};

}  // namespace managers
}  // namespace fortico
