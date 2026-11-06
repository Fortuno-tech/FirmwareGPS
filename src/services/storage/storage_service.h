#pragma once

#include <cstddef>
#include <cstdint>

#include "config/security_config.h"
#include "interfaces/invs_driver.h"
#include "interfaces/isd_driver.h"
#include "interfaces/iservice.h"
#include "services/security/badge_hasher.h"
#include "services/storage/storage_paths.h"
#include "types/storage_types.h"

namespace fortico {
namespace services {

class StorageService : public interfaces::IService {
public:
	static constexpr std::size_t MaxBadges = 2U;
	StorageService(interfaces::INvsDriver& nvsDriver,
				   const IBadgeHasher& badgeHasher,
				   interfaces::ISdDriver* sdDriver = nullptr,
				   bool enableNvsMigration =
					   security::NVS_BADGE_MIGRATION_ENABLED);
	bool start() override;
	void stop() override;
	bool isRunning() const override;

	bool addBadge(const char* normalizedUid);
	bool removeBadge(const char* normalizedUid);
	bool isBadgeAuthorized(const char* normalizedUid) const;
	bool appendLog(const types::LogRecord& record);
	bool appendCommandAudit(const char* id, const char* command,
							uint64_t sequence, bool accepted,
							const char* reason, uint64_t timestamp);

private:
	interfaces::INvsDriver& _nvsDriver;
	const IBadgeHasher& _badgeHasher;
	interfaces::ISdDriver* _sdDriver;
	bool _enableNvsMigration;
	bool _versionedWhitelist;
	uint8_t _badges[MaxBadges][IBadgeHasher::DigestSize];
	uint32_t _badgeCount;
	bool _running;
	bool loadBadges();
	bool saveBadges() const;
};

}  // namespace services
}  // namespace fortico
