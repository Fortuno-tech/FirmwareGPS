#pragma once

#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace fortico {
namespace core {

class Scheduler {
public:
	using Callback = void (*)(void* context);
	Scheduler();
	bool add(uint32_t periodMs, Callback callback, void* context);
	bool start();
	bool isRunning() const;

private:
	struct Entry { uint32_t periodMs; Callback callback; void* context; TickType_t lastTick; bool used; };
	static constexpr uint8_t kMaxEntries = 16U;
	static void taskEntry(void* context);
	void run();
	Entry _entries[kMaxEntries];
	TaskHandle_t _task = nullptr;
	bool _running = false;
};

}  // namespace core
}  // namespace fortico
