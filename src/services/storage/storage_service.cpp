#include "services/storage/storage_service.h"

#include <cstdio>
#include <cstring>

#include "config/security_config.h"

namespace fortico {
namespace services {
namespace {

constexpr const char* kLegacySharedBadgeUid = "91:51:E2:06";

struct VersionedBadgeWhitelist {
	uint32_t magic;
	uint16_t version;
	uint16_t count;
	uint8_t badges[StorageService::MaxBadges][IBadgeHasher::DigestSize];
};

static_assert(sizeof(VersionedBadgeWhitelist) ==
			  sizeof(uint32_t) * 2U +
				  StorageService::MaxBadges * IBadgeHasher::DigestSize,
			  "versioned badge whitelist layout changed");

bool validAuditText(const char* value)
{
	for (const char* cursor = value; *cursor != '\0'; ++cursor) {
		const char ch = *cursor;
		if (!((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
			  (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' ||
			  ch == '.')) return false;
	}
	return true;
}

}  // namespace

StorageService::StorageService(interfaces::INvsDriver& nvsDriver,
							   const IBadgeHasher& badgeHasher,
							   interfaces::ISdDriver* sdDriver,
							   bool enableNvsMigration)
	: _nvsDriver(nvsDriver), _badgeHasher(badgeHasher), _sdDriver(sdDriver),
	  _enableNvsMigration(enableNvsMigration), _versionedWhitelist(false),
	  _badges{}, _badgeCount(0U), _running(false) {}

bool StorageService::start()
{
	if (!_badgeHasher.isRunning() || !_nvsDriver.isInitialized()) return false;
	if (!_nvsDriver.open(security::NVS_NAMESPACE, false)) return false;
	const bool loaded = loadBadges();
	_nvsDriver.close();
	_running = loaded;
	if (!_running) return false;
	if (_enableNvsMigration && !_versionedWhitelist) {
		_versionedWhitelist = true;
		if (!saveBadges()) {
			_running = false;
			return false;
		}
	}
	if (isBadgeAuthorized(kLegacySharedBadgeUid))
		_running = removeBadge(kLegacySharedBadgeUid);
	return _running;
}

void StorageService::stop() { _running = false; }
bool StorageService::isRunning() const { return _running; }

bool StorageService::loadBadges()
{
	VersionedBadgeWhitelist versioned{};
	if (_nvsDriver.getBlob(security::NVS_KEY_RFID_WHITELIST, &versioned,
						   sizeof(versioned))) {
		if (versioned.magic != security::NVS_RFID_WHITELIST_MAGIC ||
			versioned.version != security::NVS_RFID_WHITELIST_VERSION ||
			versioned.count > MaxBadges) return false;
		_badgeCount = versioned.count;
		std::memcpy(_badges, versioned.badges, sizeof(_badges));
		_versionedWhitelist = true;
		return true;
	}

	uint8_t legacy[sizeof(_badges) + sizeof(uint32_t)]{};
	if (!_nvsDriver.getBlob(security::NVS_KEY_RFID_WHITELIST, legacy,
							sizeof(legacy))) {
		_badgeCount = 0U;
		_versionedWhitelist = false;
		return true;
	}
	std::memcpy(&_badgeCount, legacy, sizeof(_badgeCount));
	if (_badgeCount > MaxBadges) return false;
	std::memcpy(_badges, legacy + sizeof(_badgeCount), sizeof(_badges));
	_versionedWhitelist = false;
	return true;
}

bool StorageService::saveBadges() const
{
	if (_versionedWhitelist || _enableNvsMigration) {
		VersionedBadgeWhitelist versioned{};
		versioned.magic = security::NVS_RFID_WHITELIST_MAGIC;
		versioned.version = security::NVS_RFID_WHITELIST_VERSION;
		versioned.count = static_cast<uint16_t>(_badgeCount);
		std::memcpy(versioned.badges, _badges, sizeof(_badges));
		if (!_nvsDriver.open(security::NVS_NAMESPACE, false)) return false;
		const bool saved = _nvsDriver.putBlob(
			security::NVS_KEY_RFID_WHITELIST, &versioned,
			sizeof(versioned));
		_nvsDriver.close();
		return saved;
	}

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
	uint8_t digest[IBadgeHasher::DigestSize]{};
	if (!_running || !_badgeHasher.hashBadge(normalizedUid, digest, sizeof(digest))) return false;
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
	uint8_t digest[IBadgeHasher::DigestSize]{};
	if (!_running || !_badgeHasher.hashBadge(normalizedUid, digest, sizeof(digest)) ||
		isBadgeAuthorized(normalizedUid) || _badgeCount >= MaxBadges) return false;
	std::memcpy(_badges[_badgeCount++], digest, sizeof(digest));
	if (saveBadges()) return true;
	--_badgeCount;
	std::memset(_badges[_badgeCount], 0, sizeof(_badges[_badgeCount]));
	return false;
}

bool StorageService::removeBadge(const char* normalizedUid)
{
	uint8_t digest[IBadgeHasher::DigestSize]{};
	if (!_running || !_badgeHasher.hashBadge(normalizedUid, digest, sizeof(digest))) return false;
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
	if (!storage::ensureLogDirectories(*_sdDriver)) {
		return false;
	}
	char line[256]{};
	const int length = std::snprintf(line, sizeof(line),
		"{\"seq\":%lu,\"ts\":%llu,\"type\":%u,\"fuel\":%.2f}\n",
		static_cast<unsigned long>(record.seq),
		static_cast<unsigned long long>(record.timestamp),
		static_cast<unsigned int>(record.type), record.fuelPercentage);
	return length > 0 && static_cast<std::size_t>(length) < sizeof(line) &&
	       _sdDriver->append(storage::kJsonlLogPath, line,
						 static_cast<std::size_t>(length));
}

bool StorageService::appendCommandAudit(const char* id, const char* command,
										uint64_t sequence, bool accepted,
										const char* reason, uint64_t timestamp)
{
	if (!_running || _sdDriver == nullptr || id == nullptr ||
		command == nullptr || reason == nullptr) return false;
	if (!validAuditText(id) || !validAuditText(command) ||
		!validAuditText(reason)) return false;
	if (!storage::ensureLogDirectories(*_sdDriver)) return false;
	char line[256]{};
	const int length = std::snprintf(
		line, sizeof(line),
		"{\"event\":\"mqtt_command\",\"ts\":%llu,\"id\":\"%s\","
		"\"command\":\"%s\",\"seq\":%llu,\"accepted\":%s,"
		"\"reason\":\"%s\"}\n",
		static_cast<unsigned long long>(timestamp), id, command,
		static_cast<unsigned long long>(sequence),
		accepted ? "true" : "false", reason);
	return length > 0 && static_cast<std::size_t>(length) < sizeof(line) &&
		   _sdDriver->append(storage::kJsonlLogPath, line,
							 static_cast<std::size_t>(length));
}

}  // namespace services
}  // namespace fortico
