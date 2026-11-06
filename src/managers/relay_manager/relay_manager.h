#pragma once

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

private:
	drivers::RelayDriver& _driver;
	types::RelayState _state;
	types::RelayCommand _pending;
	bool _initialized;
	bool _pendingCommand;
};

}  // namespace managers
}  // namespace fortico
