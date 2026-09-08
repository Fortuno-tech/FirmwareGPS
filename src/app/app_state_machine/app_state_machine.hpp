#pragma once

#include "core/power/power_monitor.h"
#include "core/state_machine/state_machine.h"
#include "types/system_types.h"

namespace fortico {
namespace app {

class AppStateMachine {
public:
	bool start();
	bool enterInitialization();
	bool enterOperational();
	bool enterNetwork();
	bool enterSleep();
	bool enterError();
	void update(core::PowerState powerState, bool hasError);
	types::SystemState state() const;
	bool isRunning() const;

private:
	core::StateMachine _machine;
};

}  // namespace app
}  // namespace fortico
