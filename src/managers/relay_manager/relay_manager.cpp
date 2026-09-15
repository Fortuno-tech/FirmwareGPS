#include "managers/relay_manager/relay_manager.h"

namespace fortico {
namespace managers {

RelayManager::RelayManager(drivers::RelayDriver& driver)
	: _driver(driver), _state(types::RelayState::LOCKED), _pending{},
	  _initialized(false), _pendingCommand(false)
{
}

void RelayManager::init()
{
	_initialized = (_driver.init() == constants::ErrorCode::OK ||
					_driver.isInitialized());
	_state = types::RelayState::LOCKED;
	_driver.cut();
}

void RelayManager::update()
{
	if (!_initialized) return;
	_driver.poll();
	if (!_driver.isHealthy()) _state = types::RelayState::EMERGENCY_CUT;
}

void RelayManager::process()
{
	if (!_initialized || !_pendingCommand) return;
	switch (_pending.cmd) {
		case types::RelayCmd::ENGAGE:
			if (_state != types::RelayState::EMERGENCY_CUT &&
				_driver.engage()) {
				_state = types::RelayState::ENGAGED;
			}
			break;
		case types::RelayCmd::CUT:
			if (_driver.cut()) _state = types::RelayState::LOCKED;
			break;
		case types::RelayCmd::EMERGENCY_CUT:
			if (_driver.cut()) _state = types::RelayState::EMERGENCY_CUT;
			break;
	}
	_pendingCommand = false;
}

void RelayManager::reset()
{
	_driver.reset();
	_state = types::RelayState::LOCKED;
	_pendingCommand = false;
	_driver.cut();
}

bool RelayManager::submit(const types::RelayCommand& command)
{
	if (!_initialized) return false;
	_pending = command;
	_pendingCommand = true;
	return true;
}

types::RelayState RelayManager::state() const { return _state; }
bool RelayManager::hasPendingCommand() const { return _pendingCommand; }

}  // namespace managers
}  // namespace fortico
