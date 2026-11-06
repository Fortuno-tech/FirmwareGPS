#include "services/telemetry/telemetry_outbox.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace fortico {
namespace services {
namespace {

constexpr uint32_t RECORD_MAGIC = 0x58424F54U;
constexpr uint32_t METADATA_MAGIC = 0x4D42544FU;
constexpr uint16_t RECORD_VERSION_LEGACY = 1U;
constexpr uint16_t RECORD_VERSION = 2U;
constexpr uint16_t METADATA_VERSION = 1U;
constexpr size_t LEGACY_RECORD_HEADER_SIZE = 24U;
constexpr size_t RECORD_HEADER_SIZE = 28U;
constexpr size_t METADATA_SIZE = 20U;
constexpr size_t MAX_TOPIC_LENGTH = 95U;
constexpr char LEGACY_TOPIC[] = "";

void write16(uint8_t* output, uint16_t value)
{
    output[0] = static_cast<uint8_t>(value);
    output[1] = static_cast<uint8_t>(value >> 8U);
}

void write32(uint8_t* output, uint32_t value)
{
    for (uint8_t index = 0U; index < 4U; ++index) {
        output[index] = static_cast<uint8_t>(value >> (index * 8U));
    }
}

void write64(uint8_t* output, uint64_t value)
{
    for (uint8_t index = 0U; index < 8U; ++index) {
        output[index] = static_cast<uint8_t>(value >> (index * 8U));
    }
}

uint16_t read16(const uint8_t* input)
{
    return static_cast<uint16_t>(input[0]) |
           static_cast<uint16_t>(static_cast<uint16_t>(input[1]) << 8U);
}

uint32_t read32(const uint8_t* input)
{
    uint32_t value = 0U;
    for (uint8_t index = 0U; index < 4U; ++index) {
        value |= static_cast<uint32_t>(input[index]) << (index * 8U);
    }
    return value;
}

uint64_t read64(const uint8_t* input)
{
    uint64_t value = 0U;
    for (uint8_t index = 0U; index < 8U; ++index) {
        value |= static_cast<uint64_t>(input[index]) << (index * 8U);
    }
    return value;
}

uint32_t crc32(const uint8_t* data, size_t length, uint32_t crc = 0xFFFFFFFFU)
{
    for (size_t index = 0U; index < length; ++index) {
        crc ^= data[index];
        for (uint8_t bit = 0U; bit < 8U; ++bit) {
            const uint32_t mask = 0U - (crc & 1U);
            crc = (crc >> 1U) ^ (0xEDB88320U & mask);
        }
    }
    return crc;
}

uint32_t finishCrc(uint32_t crc)
{
    return crc ^ 0xFFFFFFFFU;
}

bool validMetadata(const std::vector<uint8_t>& contents, uint64_t& highWater)
{
    if (contents.size() != METADATA_SIZE ||
        read32(contents.data()) != METADATA_MAGIC ||
        read16(contents.data() + 4U) != METADATA_VERSION ||
        read16(contents.data() + 6U) != 0U) {
        return false;
    }
    if (finishCrc(crc32(contents.data(), 16U)) !=
        read32(contents.data() + 16U)) {
        return false;
    }
    highWater = read64(contents.data() + 8U);
    return true;
}

bool validRecord(const std::vector<uint8_t>& contents, OutboxMessage& message)
{
    if (contents.size() < LEGACY_RECORD_HEADER_SIZE ||
        read32(contents.data()) != RECORD_MAGIC) {
        return false;
    }

    const uint16_t version = read16(contents.data() + 4U);
    if (version == RECORD_VERSION_LEGACY) {
        if (read16(contents.data() + 6U) != LEGACY_RECORD_HEADER_SIZE) {
            return false;
        }
        const uint32_t payloadLength = read32(contents.data() + 16U);
        if (static_cast<uint64_t>(payloadLength) +
                LEGACY_RECORD_HEADER_SIZE != contents.size()) {
            return false;
        }
        uint32_t checksum = crc32(contents.data(), 20U);
        checksum = finishCrc(crc32(contents.data() + LEGACY_RECORD_HEADER_SIZE,
                                   payloadLength, checksum));
        if (checksum != read32(contents.data() + 20U)) {
            return false;
        }
        message.topic = LEGACY_TOPIC;
        message.sequence = read64(contents.data() + 8U);
        message.payload.assign(contents.begin() + LEGACY_RECORD_HEADER_SIZE,
                               contents.end());
        return true;
    }

    if (version != RECORD_VERSION ||
        read16(contents.data() + 6U) != RECORD_HEADER_SIZE) {
        return false;
    }
    const uint16_t topicLength = read16(contents.data() + 16U);
    const uint32_t payloadLength = read32(contents.data() + 20U);
    if (topicLength == 0U || topicLength > MAX_TOPIC_LENGTH ||
        read16(contents.data() + 18U) != 0U ||
        static_cast<uint64_t>(RECORD_HEADER_SIZE) + topicLength +
                payloadLength != contents.size()) {
        return false;
    }
    uint32_t checksum = crc32(contents.data(), 24U);
    checksum = crc32(contents.data() + RECORD_HEADER_SIZE,
                     topicLength, checksum);
    checksum = finishCrc(crc32(contents.data() + RECORD_HEADER_SIZE +
                                   topicLength,
                               payloadLength, checksum));
    if (checksum != read32(contents.data() + 24U)) {
        return false;
    }

    message.sequence = read64(contents.data() + 8U);
    message.topic.assign(
        reinterpret_cast<const char*>(contents.data() + RECORD_HEADER_SIZE),
        topicLength);
    message.payload.assign(contents.begin() + RECORD_HEADER_SIZE + topicLength,
                           contents.end());
    return true;
}

}  // namespace

TelemetryOutbox::TelemetryOutbox(OutboxStorage& storage, const char* directory,
                                 OutboxFullPolicy fullPolicy)
    : _storage(storage), _directory(directory == nullptr ? "" : directory),
      _fullPolicy(fullPolicy)
{
}

std::string TelemetryOutbox::recordPath(uint64_t sequence,
                                        const char* suffix) const
{
    char name[48]{};
    std::snprintf(name, sizeof(name), "record_%llu.%s",
                  static_cast<unsigned long long>(sequence), suffix);
    return _directory + "/" + name;
}

std::string TelemetryOutbox::metadataPath(int slot) const
{
    char name[16]{};
    std::snprintf(name, sizeof(name), "sequence_%d.bin", slot);
    return _directory + "/" + name;
}

bool TelemetryOutbox::loadMetadata(uint64_t& highWater, int& activeSlot,
                                   bool& metadataExists)
{
    highWater = 0U;
    activeSlot = -1;
    metadataExists = false;
    bool validFound = false;
    bool corruptFound = false;

    for (int slot = 0; slot < 2; ++slot) {
        const std::string path = metadataPath(slot);
        if (!_storage.exists(path.c_str())) {
            continue;
        }
        metadataExists = true;
        std::vector<uint8_t> contents;
        uint64_t candidate = 0U;
        if (!_storage.readFile(path.c_str(), contents)) {
            return false;
        }
        if (!validMetadata(contents, candidate)) {
            corruptFound = true;
            continue;
        }
        if (!validFound || candidate > highWater) {
            highWater = candidate;
            activeSlot = slot;
        }
        validFound = true;
    }
    if (validFound) {
        return true;
    }
    return !metadataExists && !corruptFound;
}

bool TelemetryOutbox::persistMetadata(uint64_t highWater, int activeSlot)
{
    const int nextSlot = activeSlot == 0 ? 1 : 0;
    const std::string path = metadataPath(nextSlot);
    const std::string temporaryPath = path + ".tmp";
    uint8_t contents[METADATA_SIZE]{};
    write32(contents, METADATA_MAGIC);
    write16(contents + 4U, METADATA_VERSION);
    write16(contents + 6U, 0U);
    write64(contents + 8U, highWater);
    write32(contents + 16U, finishCrc(crc32(contents, 16U)));

    if (!_storage.writeFile(temporaryPath.c_str(), contents, sizeof(contents))) {
        return false;
    }
    std::vector<uint8_t> verification;
    uint64_t verifiedValue = 0U;
    if (!_storage.readFile(temporaryPath.c_str(), verification) ||
        !validMetadata(verification, verifiedValue) ||
        verifiedValue != highWater) {
        return false;
    }
    if (_storage.exists(path.c_str()) && !_storage.removeFile(path.c_str())) {
        return false;
    }
    if (!_storage.renameFile(temporaryPath.c_str(), path.c_str())) {
        return false;
    }
    _activeMetadataSlot = nextSlot;
    _highWater = highWater;
    return true;
}

bool TelemetryOutbox::parseRecordName(const std::string& name,
                                      uint64_t& sequence,
                                      bool& temporary) const
{
    constexpr char prefix[] = "record_";
    if (name.compare(0U, sizeof(prefix) - 1U, prefix) != 0) {
        return false;
    }
    const size_t suffixPosition = name.find('.', sizeof(prefix) - 1U);
    if (suffixPosition == std::string::npos) {
        return false;
    }
    const std::string digits = name.substr(sizeof(prefix) - 1U,
                                           suffixPosition - (sizeof(prefix) - 1U));
    if (digits.empty()) {
        return false;
    }
    for (char digit : digits) {
        if (digit < '0' || digit > '9') {
            return false;
        }
    }
    char* end = nullptr;
    const unsigned long long parsed = std::strtoull(digits.c_str(), &end, 10);
    if (end == digits.c_str() || *end != '\0') {
        return false;
    }
    const std::string suffix = name.substr(suffixPosition + 1U);
    if (suffix != "msg" && suffix != "tmp") {
        return false;
    }
    sequence = static_cast<uint64_t>(parsed);
    temporary = suffix == "tmp";
    return true;
}

bool TelemetryOutbox::readRecord(const std::string& path,
                                 OutboxMessage& message)
{
    std::vector<uint8_t> contents;
    return _storage.readFile(path.c_str(), contents) &&
           validRecord(contents, message);
}

bool TelemetryOutbox::recoverFiles(uint64_t& highestSequence, bool& hasRecord)
{
    std::vector<std::string> names;
    if (!_storage.listFiles(_directory.c_str(), names)) {
        return false;
    }

    highestSequence = 0U;
    hasRecord = false;
    std::vector<std::pair<uint64_t, std::string>> temporaryFiles;
    for (const std::string& name : names) {
        uint64_t sequence = 0U;
        bool temporary = false;
        if (!parseRecordName(name, sequence, temporary)) {
            continue;
        }
        highestSequence = std::max(highestSequence, sequence);
        hasRecord = true;
        const std::string path = _directory + "/" + name;
        if (temporary) {
            temporaryFiles.emplace_back(sequence, path);
        } else {
            OutboxMessage message;
            if (!readRecord(path, message) || message.sequence != sequence) {
                return false;
            }
        }
    }

    for (const auto& entry : temporaryFiles) {
        OutboxMessage message;
        if (readRecord(entry.second, message) &&
            message.sequence == entry.first) {
            const std::string finalPath = recordPath(entry.first, "msg");
            if (_storage.exists(finalPath.c_str()) ||
                !_storage.renameFile(entry.second.c_str(), finalPath.c_str())) {
                return false;
            }
        } else if (!_storage.removeFile(entry.second.c_str())) {
            return false;
        }
    }
    return true;
}

OutboxResult TelemetryOutbox::begin()
{
    _ready = false;
    if (_directory.empty() || !_storage.createDirectory(_directory.c_str())) {
        return OutboxResult::IO_ERROR;
    }

    uint64_t storedHighWater = 0U;
    int activeSlot = -1;
    bool metadataExists = false;
    if (!loadMetadata(storedHighWater, activeSlot, metadataExists)) {
        return OutboxResult::CORRUPT;
    }

    uint64_t highestFileSequence = 0U;
    bool hasRecord = false;
    if (!recoverFiles(highestFileSequence, hasRecord)) {
        return OutboxResult::CORRUPT;
    }

    if (!metadataExists) {
        if (!persistMetadata(highestFileSequence, -1)) {
            return OutboxResult::IO_ERROR;
        }
    } else {
        _highWater = storedHighWater;
        _activeMetadataSlot = activeSlot;
        if (highestFileSequence > _highWater &&
            !persistMetadata(highestFileSequence, _activeMetadataSlot)) {
            return OutboxResult::IO_ERROR;
        }
    }
    (void)hasRecord;
    _ready = true;
    return OutboxResult::OK;
}

bool TelemetryOutbox::makeRoom(size_t requiredBytes)
{
    size_t available = 0U;
    if (!_storage.availableBytes(available)) {
        return true;
    }

    while (available < requiredBytes) {
        if (_fullPolicy != OutboxFullPolicy::OVERWRITE_OLDEST) {
            return true;
        }

        OutboxMessage oldest;
        if (peek(oldest) != OutboxResult::OK ||
            acknowledge(oldest.sequence) != OutboxResult::OK ||
            !_storage.availableBytes(available)) {
            return false;
        }
    }
    return true;
}

OutboxResult TelemetryOutbox::enqueue(const char* topic, const uint8_t* payload,
                                      size_t length, uint64_t& sequence)
{
    if (!_ready) {
        return OutboxResult::IO_ERROR;
    }
    if (topic == nullptr || topic[0] == '\0' || payload == nullptr) {
        return OutboxResult::INVALID_ARGUMENT;
    }
    const size_t topicLength = std::strlen(topic);
    if (topicLength > MAX_TOPIC_LENGTH ||
        length > std::numeric_limits<uint32_t>::max()) {
        return OutboxResult::INVALID_ARGUMENT;
    }
    if (_highWater == std::numeric_limits<uint64_t>::max()) {
        return OutboxResult::SEQUENCE_EXHAUSTED;
    }
    const size_t recordSize = RECORD_HEADER_SIZE + topicLength + length;
    if (!makeRoom(std::max(recordSize, METADATA_SIZE))) {
        return OutboxResult::IO_ERROR;
    }

    const uint64_t nextSequence = _highWater + 1U;
    if (!persistMetadata(nextSequence, _activeMetadataSlot)) {
        return OutboxResult::IO_ERROR;
    }

    const std::string temporaryPath = recordPath(nextSequence, "tmp");
    const std::string finalPath = recordPath(nextSequence, "msg");
    std::vector<uint8_t> record(RECORD_HEADER_SIZE + topicLength + length, 0U);
    write32(record.data(), RECORD_MAGIC);
    write16(record.data() + 4U, RECORD_VERSION);
    write16(record.data() + 6U, static_cast<uint16_t>(RECORD_HEADER_SIZE));
    write64(record.data() + 8U, nextSequence);
    write16(record.data() + 16U, static_cast<uint16_t>(topicLength));
    write16(record.data() + 18U, 0U);
    write32(record.data() + 20U, static_cast<uint32_t>(length));
    std::memcpy(record.data() + RECORD_HEADER_SIZE, topic, topicLength);
    std::memcpy(record.data() + RECORD_HEADER_SIZE + topicLength,
                payload, length);
    uint32_t checksum = crc32(record.data(), 24U);
    checksum = crc32(record.data() + RECORD_HEADER_SIZE, topicLength, checksum);
    checksum = finishCrc(crc32(record.data() + RECORD_HEADER_SIZE + topicLength,
                               length, checksum));
    write32(record.data() + 24U, checksum);

    if (!_storage.writeFile(temporaryPath.c_str(), record.data(),
                            record.size())) {
        if (_storage.exists(temporaryPath.c_str())) {
            _storage.removeFile(temporaryPath.c_str());
        }
        return OutboxResult::IO_ERROR;
    }

    OutboxMessage verification;
    if (!readRecord(temporaryPath, verification) ||
        verification.sequence != nextSequence ||
        verification.topic != topic ||
        verification.payload.size() != length) {
        if (_storage.exists(temporaryPath.c_str())) {
            _storage.removeFile(temporaryPath.c_str());
        }
        return OutboxResult::IO_ERROR;
    }
    if (!_storage.renameFile(temporaryPath.c_str(), finalPath.c_str())) {
        return OutboxResult::IO_ERROR;
    }

    sequence = nextSequence;
    return OutboxResult::OK;
}

OutboxResult TelemetryOutbox::peek(OutboxMessage& message)
{
    if (!_ready) {
        return OutboxResult::IO_ERROR;
    }
    std::vector<std::string> names;
    if (!_storage.listFiles(_directory.c_str(), names)) {
        return OutboxResult::IO_ERROR;
    }

    bool found = false;
    uint64_t oldestSequence = 0U;
    std::string oldestPath;
    for (const std::string& name : names) {
        uint64_t sequence = 0U;
        bool temporary = false;
        if (!parseRecordName(name, sequence, temporary) || temporary) {
            continue;
        }
        if (!found || sequence < oldestSequence) {
            found = true;
            oldestSequence = sequence;
            oldestPath = _directory + "/" + name;
        }
    }
    if (!found) {
        return OutboxResult::EMPTY;
    }
    if (!readRecord(oldestPath, message) ||
        message.sequence != oldestSequence) {
        return OutboxResult::CORRUPT;
    }
    return OutboxResult::OK;
}

OutboxResult TelemetryOutbox::acknowledge(uint64_t sequence)
{
    OutboxMessage oldest;
    const OutboxResult result = peek(oldest);
    if (result != OutboxResult::OK) {
        return result;
    }
    if (oldest.sequence != sequence) {
        return OutboxResult::INVALID_ARGUMENT;
    }
    const std::string path = recordPath(sequence, "msg");
    return _storage.removeFile(path.c_str()) ? OutboxResult::OK
                                             : OutboxResult::IO_ERROR;
}

OutboxResult TelemetryOutbox::publishNext(PublishFunction publish,
                                          void* context,
                                          uint64_t* publishedSequence)
{
    if (publish == nullptr) {
        return OutboxResult::INVALID_ARGUMENT;
    }
    OutboxMessage message;
    const OutboxResult result = peek(message);
    if (result != OutboxResult::OK) {
        return result;
    }
    if (!publish(context, message.sequence, message.topic.c_str(),
                 message.payload.data(),
                 message.payload.size())) {
        return OutboxResult::IO_ERROR;
    }
    const OutboxResult acknowledgement = acknowledge(message.sequence);
    if (acknowledgement != OutboxResult::OK) {
        return acknowledgement;
    }
    if (publishedSequence != nullptr) {
        *publishedSequence = message.sequence;
    }
    return OutboxResult::OK;
}

}  // namespace services
}  // namespace fortico
