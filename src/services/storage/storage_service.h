#pragma once

#include <cstddef>
#include <cstdint>

#include "interfaces/invs_driver.h"
#include "interfaces/isd_driver.h"
#include "interfaces/iservice.h"
#include "services/security/security_service.h"
#include "types/storage_types.h"

namespace fortico {
namespace services {

class StorageService : public interfaces::IService {
public:
	static constexpr std::size_t MaxBadges = 32U;
	StorageService(interfaces::INvsDriver& nvsDriver,
				   SecurityService& security,
				   interfaces::ISdDriver* sdDriver = nullptr);
	bool start() override;
	void stop() override;
	bool isRunning() const override;

	bool addBadge(const char* normalizedUid);
	bool removeBadge(const char* normalizedUid);
	bool isBadgeAuthorized(const char* normalizedUid) const;
	bool appendLog(const types::LogRecord& record);

private:
	interfaces::INvsDriver& _nvsDriver;
	SecurityService& _security;
	interfaces::ISdDriver* _sdDriver;
	uint8_t _badges[MaxBadges][SecurityService::HmacSize];
	uint32_t _badgeCount;
	bool _running;
	bool loadBadges();
	bool saveBadges() const;
};

}  // namespace services
}  // namespace fortico
