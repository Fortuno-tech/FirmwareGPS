#include "app/app_state_machine/app_state_machine.hpp"

namespace fortico {
namespace app {

bool AppStateMachine::start()
{
	return _machine.start(types::SystemState::BOOT);
}

bool AppStateMachine::enterInitialization()
{
	return _machine.transition(types::SystemState::INIT);
}

bool AppStateMachine::enterOperational()
{
	return _machine.transition(types::SystemState::OPERATIONAL);
}

bool AppStateMachine::enterNetwork()
{
	return _machine.transition(types::SystemState::NETWORK);
}

bool AppStateMachine::enterSleep()
{
	return _machine.transition(types::SystemState::SLEEP);
}

bool AppStateMachine::enterError()
{
	return _machine.transition(types::SystemState::ERROR);
}

void AppStateMachine::update(core::PowerState powerState, bool hasError)
{
	if (!_machine.isRunning()) return;
	if (hasError) {
		enterError();
	} else if (powerState == core::PowerState::CRITICAL) {
		enterSleep();
	} else if (_machine.state() == types::SystemState::SLEEP) {
		enterOperational();
	}
}

types::SystemState AppStateMachine::state() const { return _machine.state(); }
bool AppStateMachine::isRunning() const { return _machine.isRunning(); }

}  // namespace app
}  // namespace fortico
