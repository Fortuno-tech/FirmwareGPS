#include "core/state_machine/state_machine.h"

namespace fortico {
namespace core {

StateMachine::StateMachine() : _state(types::SystemState::BOOT), _running(false) {}
bool StateMachine::start(types::SystemState initial) { _state = initial; _running = true; return true; }
bool StateMachine::transition(types::SystemState next, TransitionGuard guard, void* context)
{
	if (!_running || (guard != nullptr && !guard(_state, next, context))) return false;
	_state = next;
	return true;
}
types::SystemState StateMachine::state() const { return _state; }
bool StateMachine::isRunning() const { return _running; }

}  // namespace core
}  // namespace fortico
