#include "app/task_manager/task_manager.hpp"

namespace fortico {
namespace app {

bool TaskManager::addPeriodic(uint32_t periodMs, Callback callback, void* context)
{
	return _scheduler.add(periodMs, callback, context);
}

bool TaskManager::start() { return _scheduler.start(); }
void TaskManager::stop() { _scheduler.stop(); }
bool TaskManager::isRunning() const { return _scheduler.isRunning(); }

}  // namespace app
}  // namespace fortico
