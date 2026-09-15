#pragma once

#include "interfaces/iservice.h"
#include "managers/authentication_manager/authentication_manager.hpp"

namespace fortico {
namespace services {

class AuthenticationService : public interfaces::IService {
public:
	explicit AuthenticationService(managers::AuthenticationManager& manager);

	bool start() override;
	void stop() override;
	bool isRunning() const override;

	bool authenticate(const char* uid, uint64_t timestamp = 0U);
	types::AuthenticationDecision decision() const;

private:
	managers::AuthenticationManager& _manager;
	bool _running;
};

}  // namespace services
}  // namespace fortico
