#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fortico {
namespace services {

struct OutboxMessage {
    uint64_t sequence = 0U;
    std::string topic;
    std::vector<uint8_t> payload;
};

enum class OutboxResult : uint8_t {
    OK,
    EMPTY,
    INVALID_ARGUMENT,
    IO_ERROR,
    CORRUPT,
    SEQUENCE_EXHAUSTED,
};

enum class OutboxFullPolicy : uint8_t {
    REJECT_NEW,
    OVERWRITE_OLDEST,
};

class OutboxStorage {
public:
    virtual ~OutboxStorage() = default;

    virtual bool createDirectory(const char* path) = 0;
    virtual bool listFiles(const char* path,
                           std::vector<std::string>& names) = 0;
    virtual bool exists(const char* path) = 0;
    virtual bool readFile(const char* path,
                          std::vector<uint8_t>& contents) = 0;
    virtual bool writeFile(const char* path, const uint8_t* contents,
                           size_t length) = 0;
    virtual bool renameFile(const char* from, const char* to) = 0;
    virtual bool removeFile(const char* path) = 0;
    virtual bool availableBytes(size_t& bytes) const
    {
        (void)bytes;
        return false;
    }
};

class TelemetryOutbox {
public:
    using PublishFunction = bool (*)(void* context, uint64_t sequence,
                                     const char* topic, const uint8_t* payload,
                                     size_t length);

    TelemetryOutbox(OutboxStorage& storage, const char* directory,
                    OutboxFullPolicy fullPolicy = OutboxFullPolicy::REJECT_NEW);

    OutboxResult begin();
    OutboxResult enqueue(const char* topic, const uint8_t* payload,
                         size_t length, uint64_t& sequence);
    OutboxResult peek(OutboxMessage& message);
    OutboxResult acknowledge(uint64_t sequence);
    OutboxResult publishNext(PublishFunction publish, void* context,
                             uint64_t* publishedSequence = nullptr);

private:
    bool loadMetadata(uint64_t& highWater, int& activeSlot,
                      bool& metadataExists);
    bool persistMetadata(uint64_t highWater, int activeSlot);
    bool recoverFiles(uint64_t& highestSequence, bool& hasRecord);
    bool readRecord(const std::string& path, OutboxMessage& message);
    bool makeRoom(size_t requiredBytes);
    bool parseRecordName(const std::string& name, uint64_t& sequence,
                         bool& temporary) const;
    std::string recordPath(uint64_t sequence, const char* suffix) const;
    std::string metadataPath(int slot) const;

    OutboxStorage& _storage;
    std::string _directory;
    uint64_t _highWater = 0U;
    int _activeMetadataSlot = -1;
    OutboxFullPolicy _fullPolicy;
    bool _ready = false;
};

}  // namespace services
}  // namespace fortico
