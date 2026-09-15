#pragma once

#include <cstddef>

#include "interfaces/idriver.h"
#include "interfaces/imanager.h"
#include "types/system_types.h"

namespace fortico {
namespace managers {

class DiagnosticsManager : public interfaces::IManager {
public:
	DiagnosticsManager(interfaces::IDriver* const* drivers,
					   std::size_t driverCount);

	void init() override;
	void update() override;
	void process() override;
	void reset() override;

	types::SystemHealth getHealth() const;
	types::DiagnosticReport getReport() const;
	bool allHealthy() const;
	void setRuntimeMetrics(uint32_t freeHeapBytes,
						   uint32_t minFreeHeapBytes,
						   uint16_t minStackWordsRelayTask,
						   uint8_t i2cFailures,
						   uint8_t sim7600Failures);

private:
	interfaces::IDriver* const* _drivers;
	std::size_t _driverCount;
	types::SystemHealth _health;
	types::DiagnosticReport _report;
	bool _initialized;
};

}  // namespace managers
}  // namespace fortico
