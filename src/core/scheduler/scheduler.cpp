#include "core/scheduler/scheduler.h"

#include "config/task_config.h"

namespace fortico {
namespace core {

Scheduler::Scheduler() : _entries{}
{
}

bool Scheduler::add(uint32_t periodMs, Callback callback, void* context)
{
	if (periodMs == 0U || callback == nullptr || _running) return false;
	for (Entry& entry : _entries) {
		if (!entry.used) {
			entry = {periodMs, callback, context, xTaskGetTickCount(), true};
			return true;
		}
	}
	return false;
}

bool Scheduler::start()
{
	if (_running) return false;
	if (xTaskCreatePinnedToCore(&Scheduler::taskEntry, "SchedulerTask",
		tasks::SCHEDULER_TASK_STACK_WORDS, this, tasks::SCHEDULER_TASK_PRIORITY,
		&_task, tasks::CORE_NETWORK) != pdTRUE) return false;
	_running = true;
	return true;
}

bool Scheduler::isRunning() const { return _running; }

void Scheduler::taskEntry(void* context) { static_cast<Scheduler*>(context)->run(); }

void Scheduler::run()
{
	for (;;) {
		const TickType_t now = xTaskGetTickCount();
		TickType_t waitTicks = pdMS_TO_TICKS(100U);
		for (Entry& entry : _entries) {
			if (!entry.used) continue;
			if (now - entry.lastTick >= pdMS_TO_TICKS(entry.periodMs)) {
				entry.lastTick = now;
				entry.callback(entry.context);
			}
			const TickType_t elapsed = now - entry.lastTick;
			const TickType_t period = pdMS_TO_TICKS(entry.periodMs);
			if (elapsed < period && period - elapsed < waitTicks) waitTicks = period - elapsed;
		}
		vTaskDelay(waitTicks == 0U ? 1U : waitTicks);
	}
}

}  // namespace core
}  // namespace fortico
