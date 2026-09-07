#include "services/authentication/authentication_service.h"

namespace fortico {
namespace services {

AuthenticationService::AuthenticationService(
	managers::AuthenticationManager& manager)
	: _manager(manager), _running(false)
{
}

bool AuthenticationService::start()
{
	_manager.init();
	_running = true;
	return true;
}

void AuthenticationService::stop() { _running = false; }
bool AuthenticationService::isRunning() const { return _running; }

bool AuthenticationService::authenticate(const char* uid, uint64_t timestamp)
{
	return _running && _manager.authenticate(uid, timestamp);
}

types::AuthenticationDecision AuthenticationService::decision() const
{
	return _manager.decision();
}

}  // namespace services
}  // namespace fortico
