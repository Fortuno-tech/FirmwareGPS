#pragma once

#include <cstddef>

#include "interfaces/imanager.h"
#include "drivers/sd/sd_driver.h"
#include "types/storage_types.h"

namespace fortico {
namespace managers {

class StorageManager : public interfaces::IManager {
public:
	explicit StorageManager(drivers::SdDriver& driver,
							const char* logPath = "/fortico/log.jsonl");

	void init() override;
	void update() override;
	void process() override;
	void reset() override;

	bool append(const types::LogRecord& record);
	bool isReady() const;
	std::size_t droppedCount() const;

private:
	drivers::SdDriver& _driver;
	const char* _logPath;
	bool _initialized;
	bool _ready;
	std::size_t _droppedCount;
};

}  // namespace managers
}  // namespace fortico
