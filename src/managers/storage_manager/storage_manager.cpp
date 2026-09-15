#include "managers/storage_manager/storage_manager.h"

#include <cstdio>

namespace fortico {
namespace managers {

StorageManager::StorageManager(drivers::SdDriver& driver,
							   const char* logPath)
	: _driver(driver), _logPath(logPath), _initialized(false), _ready(false),
	  _droppedCount(0U)
{
}

void StorageManager::init()
{
	_initialized = (_driver.init() == constants::ErrorCode::OK ||
					_driver.isInitialized());
	_ready = _initialized && _driver.isHealthy();
}

void StorageManager::update()
{
	if (!_initialized) return;
	_driver.poll();
	_ready = _driver.isHealthy();
	if (_ready) _driver.flush();
}

void StorageManager::process() { update(); }

void StorageManager::reset()
{
	_driver.reset();
	_ready = _driver.isHealthy();
}

bool StorageManager::append(const types::LogRecord& record)
{
	if (!_initialized || !_ready || _logPath == nullptr) {
		++_droppedCount;
		return false;
	}
	char line[256];
	const int length = std::snprintf(
		line, sizeof(line),
		"{\"seq\":%lu,\"timestamp\":%llu,\"type\":%u,\"fuelPercentage\":%.2f}\n",
		static_cast<unsigned long>(record.seq),
		static_cast<unsigned long long>(record.timestamp),
		static_cast<unsigned int>(record.type), record.fuelPercentage);
	if (length <= 0 || static_cast<std::size_t>(length) >= sizeof(line) ||
		!_driver.append(_logPath, line, static_cast<std::size_t>(length))) {
		++_droppedCount;
		return false;
	}
	return true;
}

bool StorageManager::isReady() const { return _ready; }
std::size_t StorageManager::droppedCount() const { return _droppedCount; }

}  // namespace managers
}  // namespace fortico
