#pragma once

#include <cstdint>

namespace fortico {
namespace core {

class MemoryMonitor {
public:
	bool init();
	void sample();
	uint32_t freeHeap() const;
	uint32_t minimumFreeHeap() const;
	bool withinBudget(uint8_t minimumPercent = 20U) const;

private:
	uint32_t _freeHeap = 0U;
	uint32_t _minimumFreeHeap = 0U;
	uint32_t _totalHeap = 0U;
	bool _initialized = false;
};

}  // namespace core
}  // namespace fortico
