#pragma once

#include <cstdint>

#include "core/scheduler/scheduler.h"

namespace fortico {
namespace app {

class TaskManager {
public:
	using Callback = core::Scheduler::Callback;

	bool addPeriodic(uint32_t periodMs, Callback callback, void* context);
	bool start();
	void stop();
	bool isRunning() const;

private:
	core::Scheduler _scheduler;
};

}  // namespace app
}  // namespace fortico
