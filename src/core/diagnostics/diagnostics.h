#pragma once

#include "core/memory/memory_monitor.h"
#include "types/system_types.h"

namespace fortico {
namespace core {

class Diagnostics {
public:
	explicit Diagnostics(MemoryMonitor& memoryMonitor);
	void init();
	void sample();
	types::SystemHealth health() const;
	bool healthy() const;

private:
	MemoryMonitor& _memoryMonitor;
	types::SystemHealth _health;
	bool _initialized;
};

}  // namespace core
}  // namespace fortico
