#include "managers/authentication_manager/authentication_manager.hpp"

#include <cstring>

#include "constants/events.h"
#include "services/storage/storage_service.h"

namespace fortico {
namespace managers {

AuthenticationManager::AuthenticationManager(interfaces::IPublisher* publisher)
	: _publisher(publisher), _storageService(nullptr), _decision{}, _initialized(false)
{
	reset();
}

void AuthenticationManager::init()
{
	_initialized = true;
	reset();
}

void AuthenticationManager::update() {}
void AuthenticationManager::process() {}
void AuthenticationManager::setCredentialStore(services::StorageService* storageService)
{
	_storageService = storageService;
}

void AuthenticationManager::reset()
{
	_decision = {};
	_decision.result = types::AuthenticationResult::UNKNOWN_CARD;
	_decision.state = types::AuthState::UNAUTHENTICATED;
}

bool AuthenticationManager::authenticate(const char* uid, uint64_t timestamp)
{
	if (!_initialized || uid == nullptr || uid[0] == '\0') return false;

	_decision = {};
	std::strncpy(_decision.uid, uid, sizeof(_decision.uid) - 1U);
	_decision.timestamp = timestamp;
	_decision.result = types::AuthenticationResult::UNKNOWN_CARD;
	_decision.state = types::AuthState::UNAUTHENTICATED;

	const bool authorized = _storageService != nullptr &&
		_storageService->isRunning() && _storageService->isBadgeAuthorized(uid);

	if (authorized) {
		_decision.result = types::AuthenticationResult::AUTHORIZED;
		_decision.state = types::AuthState::AUTHENTICATED;
	}

	if (_publisher != nullptr) {
		const constants::EventID eventId =
			_decision.result == types::AuthenticationResult::AUTHORIZED
				? constants::EventID::AUTH_AUTHORIZED
				: constants::EventID::AUTH_DENIED;
		_publisher->publish({eventId, timestamp, 0U});
	}
	return authorized;
}

types::AuthenticationDecision AuthenticationManager::decision() const
{
	return _decision;
}

types::AuthState AuthenticationManager::state() const
{
	return _decision.state;
}

}  // namespace managers
}  // namespace fortico
