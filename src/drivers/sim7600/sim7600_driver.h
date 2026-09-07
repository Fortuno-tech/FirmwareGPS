#pragma once

#include <cstddef>
#include <cstdint>

#include "constants/errors.h"
#include "hal/uart/uart_hal.h"
#include "interfaces/idriver.h"

namespace fortico {
namespace drivers {

class Sim7600Driver : public interfaces::IDriver {
public:
	explicit Sim7600Driver(hal::UartHal& uartHal);

	constants::ErrorCode init() override;
	constants::ErrorCode poll() override;
	bool start() override;
	bool stop() override;
	bool isHealthy() const override;
	bool isInitialized() const override;
	void reset() override;

	bool sendCommand(const char* command, char* response,
					 std::size_t responseSize, uint32_t timeoutMs);
	bool writeRaw(const uint8_t* data, std::size_t length,
				  char* response, std::size_t responseSize,
				  uint32_t timeoutMs);
	bool testAt(uint32_t timeoutMs = 1000U);

private:
	hal::UartHal& _uartHal;
	bool _initialized;
	bool _healthy;
	bool _active;
};

}  // namespace drivers
}  // namespace fortico
