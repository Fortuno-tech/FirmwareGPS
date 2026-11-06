#include "core/scheduler/scheduler.h"

#include "config/task_config.h"

namespace fortico {
namespace core {

Scheduler::Scheduler() : _entries{}, _startGate(xSemaphoreCreateBinary())
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
	if (_startGate == nullptr) return false;
	(void)xSemaphoreTake(_startGate, 0U);
	if (xTaskCreatePinnedToCore(&Scheduler::taskEntry, "SchedulerTask",
		tasks::SCHEDULER_TASK_STACK_WORDS, this,
		tasks::SYSTEM_CONTROLLER_TASK_PRIORITY, &_task,
		tasks::CORE_APP_CRITICAL) != pdTRUE) return false;
	if (_watchdog != nullptr &&
		!_watchdog->registerTask(_task, "SchedulerTask")) {
		vTaskDelete(_task);
		_task = nullptr;
		return false;
	}
	_running = true;
	(void)xSemaphoreGive(_startGate);
	return true;
}

void Scheduler::stop()
{
	if (!_running) return;
	_running = false;
	if (_task != nullptr) {
		TaskHandle_t task = _task;
		_task = nullptr;
		if (_watchdog != nullptr) {
			(void)_watchdog->unregisterTask(task);
		}
		vTaskDelete(task);
	}
}

bool Scheduler::isRunning() const { return _running; }
void Scheduler::setWatchdog(Watchdog* watchdog) { _watchdog = watchdog; }

void Scheduler::taskEntry(void* context) { static_cast<Scheduler*>(context)->run(); }

void Scheduler::run()
{
	if (_startGate == nullptr ||
		xSemaphoreTake(_startGate, pdMS_TO_TICKS(1000U)) != pdTRUE ||
		!_running) {
		if (_watchdog != nullptr) {
			(void)_watchdog->unregisterTask(xTaskGetCurrentTaskHandle());
		}
		_task = nullptr;
		vTaskDelete(nullptr);
		return;
	}
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
		if (_watchdog != nullptr) {
			_watchdog->feed(xTaskGetCurrentTaskHandle());
		}
		vTaskDelay(waitTicks == 0U ? 1U : waitTicks);
	}
}

}  // namespace core
}  // namespace fortico
