#include "core/memory/memory_monitor.h"

#ifdef ESP32_TARGET
#include <esp_heap_caps.h>
#endif

namespace fortico {
namespace core {

bool MemoryMonitor::init()
{
#ifdef ESP32_TARGET
	_totalHeap = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
	_freeHeap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
#else
	_totalHeap = 0U;
	_freeHeap = 0U;
#endif
	_minimumFreeHeap = _freeHeap;
	_initialized = true;
	return true;
}

void MemoryMonitor::sample()
{
	if (!_initialized) return;
#ifdef ESP32_TARGET
	_freeHeap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
#endif
	if (_freeHeap < _minimumFreeHeap) _minimumFreeHeap = _freeHeap;
}

uint32_t MemoryMonitor::freeHeap() const { return _freeHeap; }
uint32_t MemoryMonitor::minimumFreeHeap() const { return _minimumFreeHeap; }

bool MemoryMonitor::withinBudget(uint8_t minimumPercent) const
{
	if (!_initialized || _totalHeap == 0U) return false;
	return static_cast<uint64_t>(_freeHeap) * 100ULL >=
		   static_cast<uint64_t>(_totalHeap) * minimumPercent;
}

}  // namespace core
}  // namespace fortico
