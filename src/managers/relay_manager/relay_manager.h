#pragma once

#include <cstdint>

#include "drivers/relay/relay_driver.h"
#include "interfaces/imanager.h"
#include "types/relay_types.h"

namespace fortico {
namespace managers {

class RelayManager : public interfaces::IManager {
public:
	explicit RelayManager(drivers::RelayDriver& driver);

	void init() override;
	void update() override;
	void process() override;
	void reset() override;

	bool submit(const types::RelayCommand& command);
	types::RelayState state() const;
	bool hasPendingCommand() const;
	bool lastCommandSucceeded() const;
	uint32_t processedCommandCount() const;
	const types::RelayCommand& lastProcessedCommand() const;

private:
	drivers::RelayDriver& _driver;
	types::RelayState _state;
	types::RelayCommand _pending;
	bool _initialized;
	bool _pendingCommand;
	bool _lastCommandSucceeded;
	uint32_t _processedCommandCount;
	types::RelayCommand _lastProcessedCommand;
};

}  // namespace managers
}  // namespace fortico
