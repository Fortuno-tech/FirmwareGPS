#include "core/diagnostics/diagnostics.h"

namespace fortico {
namespace core {

Diagnostics::Diagnostics(MemoryMonitor& memoryMonitor)
	: _memoryMonitor(memoryMonitor), _health{}, _initialized(false) {}

void Diagnostics::init()
{
	_memoryMonitor.init();
	_initialized = true;
	sample();
}

void Diagnostics::sample()
{
	if (!_initialized) return;
	_memoryMonitor.sample();
	_health.freeHeapBytes = _memoryMonitor.freeHeap();
	_health.minFreeHeapBytes = _memoryMonitor.minimumFreeHeap();
	_health.heapWithinBudget = _memoryMonitor.withinBudget(20U);
}

types::SystemHealth Diagnostics::health() const { return _health; }
bool Diagnostics::healthy() const { return _initialized && _health.heapWithinBudget; }

}  // namespace core
}  // namespace fortico
