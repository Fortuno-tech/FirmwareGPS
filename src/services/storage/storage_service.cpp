#include "services/storage/storage_service.h"

#include <cstdio>
#include <cstring>

#include "config/security_config.h"

namespace fortico {
namespace services {

StorageService::StorageService(interfaces::INvsDriver& nvsDriver,
							   SecurityService& security,
							   interfaces::ISdDriver* sdDriver)
	: _nvsDriver(nvsDriver), _security(security), _sdDriver(sdDriver),
	  _badges{}, _badgeCount(0U), _running(false) {}

bool StorageService::start()
{
	if (!_security.isRunning() || !_nvsDriver.isInitialized()) return false;
	if (!_nvsDriver.open(security::NVS_NAMESPACE, false)) return false;
	const bool loaded = loadBadges();
	_nvsDriver.close();
	_running = loaded;
	return _running;
}

void StorageService::stop() { _running = false; }
bool StorageService::isRunning() const { return _running; }

bool StorageService::loadBadges()
{
	uint8_t stored[sizeof(_badges) + sizeof(uint32_t)]{};
	std::size_t length = sizeof(stored);
	if (!_nvsDriver.getBlob(security::NVS_KEY_RFID_WHITELIST, stored,
							sizeof(stored))) {
		_badgeCount = 0U;
		return true;
	}
	std::memcpy(&_badgeCount, stored, sizeof(_badgeCount));
	if (_badgeCount > MaxBadges) return false;
	std::memcpy(_badges, stored + sizeof(_badgeCount), sizeof(_badges));
	(void)length;
	return true;
}

bool StorageService::saveBadges() const
{
	uint8_t stored[sizeof(_badges) + sizeof(uint32_t)]{};
	std::memcpy(stored, &_badgeCount, sizeof(_badgeCount));
	std::memcpy(stored + sizeof(_badgeCount), _badges, sizeof(_badges));
	// StorageService::start() closes its NVS session once the whitelist is
	// loaded. Reopen it for every persistent update so serial/MQTT updates
	// do not depend on an unrelated caller keeping NVS open.
	if (!_nvsDriver.open(security::NVS_NAMESPACE, false)) return false;
	const bool saved = _nvsDriver.putBlob(security::NVS_KEY_RFID_WHITELIST,
									 stored, sizeof(stored));
	_nvsDriver.close();
	return saved;
}

bool StorageService::isBadgeAuthorized(const char* normalizedUid) const
{
	uint8_t digest[SecurityService::HmacSize]{};
	if (!_running || !_security.hashBadge(normalizedUid, digest, sizeof(digest))) return false;
	for (uint32_t index = 0U; index < _badgeCount; ++index) {
		uint8_t diff = 0U;
		for (std::size_t byte = 0U; byte < sizeof(digest); ++byte)
			diff |= static_cast<uint8_t>(digest[byte] ^ _badges[index][byte]);
		if (diff == 0U) return true;
	}
	return false;
}

bool StorageService::addBadge(const char* normalizedUid)
{
	uint8_t digest[SecurityService::HmacSize]{};
	if (!_running || !_security.hashBadge(normalizedUid, digest, sizeof(digest)) ||
		isBadgeAuthorized(normalizedUid) || _badgeCount >= MaxBadges) return false;
	std::memcpy(_badges[_badgeCount++], digest, sizeof(digest));
	return saveBadges();
}

bool StorageService::removeBadge(const char* normalizedUid)
{
	uint8_t digest[SecurityService::HmacSize]{};
	if (!_running || !_security.hashBadge(normalizedUid, digest, sizeof(digest))) return false;
	for (uint32_t index = 0U; index < _badgeCount; ++index) {
		if (std::memcmp(digest, _badges[index], sizeof(digest)) == 0) {
			std::memmove(_badges[index], _badges[index + 1U],
						 (_badgeCount - index - 1U) * sizeof(_badges[0]));
			--_badgeCount;
			return saveBadges();
		}
	}
	return false;
}

bool StorageService::appendLog(const types::LogRecord& record)
{
	if (!_running || _sdDriver == nullptr) return false;
	char line[256]{};
	const int length = std::snprintf(line, sizeof(line),
		"{\"seq\":%lu,\"ts\":%llu,\"type\":%u,\"fuel\":%.2f}\n",
		static_cast<unsigned long>(record.seq),
		static_cast<unsigned long long>(record.timestamp),
		static_cast<unsigned int>(record.type), record.fuelPercentage);
	return length > 0 && static_cast<std::size_t>(length) < sizeof(line) &&
		   _sdDriver->append("/fortico/log.jsonl", line,
							 static_cast<std::size_t>(length));
}

}  // namespace services
}  // namespace fortico
