#pragma once

#include <cstddef>
#include <cstdint>

#include "interfaces/imanager.h"
#include "interfaces/ipublisher.h"
#include "types/auth_types.h"

namespace fortico {
namespace services { class StorageService; }
namespace managers {

class AuthenticationManager : public interfaces::IManager {
public:
	explicit AuthenticationManager(interfaces::IPublisher* publisher = nullptr);
	void setCredentialStore(services::StorageService* storageService);

	void init() override;
	void update() override;
	void process() override;
	void reset() override;

	bool authenticate(const char* uid, uint64_t timestamp = 0U);
	types::AuthenticationDecision decision() const;
	types::AuthState state() const;

private:
	interfaces::IPublisher* _publisher;
	services::StorageService* _storageService;
	types::AuthenticationDecision _decision;
	bool _initialized;
};

}  // namespace managers
}  // namespace fortico
