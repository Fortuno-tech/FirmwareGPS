#pragma once

#include "types/system_types.h"

namespace fortico {
namespace core {

class StateMachine {
public:
	using TransitionGuard = bool (*)(types::SystemState from, types::SystemState to, void* context);
	StateMachine();
	bool start(types::SystemState initial = types::SystemState::BOOT);
	bool transition(types::SystemState next, TransitionGuard guard = nullptr, void* context = nullptr);
	types::SystemState state() const;
	bool isRunning() const;

private:
	types::SystemState _state;
	bool _running;
};

}  // namespace core
}  // namespace fortico
