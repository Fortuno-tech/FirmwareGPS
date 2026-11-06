#pragma once

#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "core/watchdog/watchdog.h"

namespace fortico {
namespace core {

class Scheduler {
public:
	using Callback = void (*)(void* context);
	Scheduler();
	bool add(uint32_t periodMs, Callback callback, void* context);
	bool start();
	void stop();
	bool isRunning() const;
	void setWatchdog(Watchdog* watchdog);

private:
	struct Entry { uint32_t periodMs; Callback callback; void* context; TickType_t lastTick; bool used; };
	static constexpr uint8_t kMaxEntries = 16U;
	static void taskEntry(void* context);
	void run();
	Entry _entries[kMaxEntries];
	TaskHandle_t _task = nullptr;
	SemaphoreHandle_t _startGate = nullptr;
	Watchdog* _watchdog = nullptr;
	bool _running = false;
};

}  // namespace core
}  // namespace fortico
