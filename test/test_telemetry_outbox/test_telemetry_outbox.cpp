#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <unity.h>

#define assert(condition) TEST_ASSERT_TRUE(condition)

#include "services/telemetry/telemetry_outbox.h"
#include "services/telemetry/telemetry_outbox.cpp"
#include "services/mqtt/telemetry_wire_payload.h"
#include "services/mqtt/mqtt_telemetry_sink.h"
#include "services/mqtt/mqtt_topics.cpp"

using fortico::services::OutboxMessage;
using fortico::services::OutboxResult;
using fortico::services::OutboxStorage;
using fortico::services::TelemetryOutbox;
using fortico::services::ITelemetryHandoff;
using fortico::services::MqttTelemetrySink;

void setUp() {}
void tearDown() {}

namespace {

class MemoryStorage final : public OutboxStorage {
public:
    bool createDirectory(const char* path) override
    {
        if (failCreateDirectory) {
            return false;
        }
        directories[path] = true;
        return true;
    }

    bool listFiles(const char* path,
                   std::vector<std::string>& names) override
    {
        if (directories.count(path) == 0U) {
            return false;
        }
        names.clear();
        const std::string prefix = std::string(path) + "/";
        for (const auto& file : files) {
            if (file.first.compare(0U, prefix.size(), prefix) == 0) {
                names.push_back(file.first.substr(prefix.size()));
            }
        }
        return true;
    }

    bool exists(const char* path) override
    {
        return files.count(path) != 0U;
    }

    bool readFile(const char* path,
                  std::vector<uint8_t>& contents) override
    {
        const auto file = files.find(path);
        if (file == files.end()) {
            return false;
        }
        contents = file->second;
        return true;
    }

    bool writeFile(const char* path, const uint8_t* contents,
                   size_t length) override
    {
        files.erase(path);
        const size_t available = capacity > usedBytes()
                                     ? capacity - usedBytes()
                                     : 0U;
        const size_t written = std::min(length, available);
        files[path] = std::vector<uint8_t>(contents, contents + written);
        return written == length;
    }

    bool renameFile(const char* from, const char* to) override
    {
        const auto file = files.find(from);
        if (file == files.end() || files.count(to) != 0U) {
            return false;
        }
        files[to] = file->second;
        files.erase(file);
        return true;
    }

    bool removeFile(const char* path) override
    {
        if (failRemove) {
            return false;
        }
        return files.erase(path) != 0U;
    }

    size_t usedBytes() const
    {
        size_t used = 0U;
        for (const auto& file : files) {
            used += file.second.size();
        }
        return used;
    }

    std::map<std::string, std::vector<uint8_t>> files;
    std::map<std::string, bool> directories;
    size_t capacity = static_cast<size_t>(-1);
    bool failRemove = false;
    bool failCreateDirectory = false;
};

class FakeHandoff final : public ITelemetryHandoff {
public:
    explicit FakeHandoff(size_t queueCapacity) : capacity(queueCapacity) {}

    bool enqueueTelemetry(const char* topic, const void* payload,
                          size_t length) override
    {
        if (topic == nullptr || payload == nullptr || length == 0U ||
            messages.size() >= capacity) {
            return false;
        }
        messages.emplace_back(
            std::string(topic),
            std::string(static_cast<const char*>(payload), length));
        return true;
    }

    size_t capacity;
    std::vector<std::pair<std::string, std::string>> messages;
};

struct PublishState {
    bool succeed = false;
    std::vector<uint64_t> sequences;
    std::vector<std::string> topics;
    std::vector<std::vector<uint8_t>> received;
};

bool publishMessage(void* context, uint64_t sequence,
                    const char* topic, const uint8_t* payload, size_t length)
{
    auto& state = *static_cast<PublishState*>(context);
    state.sequences.push_back(sequence);
    state.topics.emplace_back(topic);
    state.received.emplace_back(payload, payload + length);
    return state.succeed;
}

void testFifoReplayAcknowledgesOnlyAfterPublish()
{
    MemoryStorage storage;
    TelemetryOutbox outbox(storage, "/outbox");
    assert(outbox.begin() == OutboxResult::OK);

    const uint8_t first[] = {'a', '1'};
    const uint8_t second[] = {'b', '2'};
    uint64_t firstSequence = 0U;
    uint64_t secondSequence = 0U;
    assert(outbox.enqueue("fleet/unit/telemetry", first, sizeof(first),
                          firstSequence) == OutboxResult::OK);
    assert(outbox.enqueue("fleet/unit/telemetry", second, sizeof(second),
                          secondSequence) == OutboxResult::OK);
    assert(firstSequence == 1U && secondSequence == 2U);

    PublishState publisher;
    assert(outbox.publishNext(publishMessage, &publisher) == OutboxResult::IO_ERROR);
    assert(publisher.sequences == std::vector<uint64_t>{firstSequence});
    OutboxMessage pending;
    assert(outbox.peek(pending) == OutboxResult::OK);
    assert(pending.sequence == firstSequence);
    assert(pending.payload == std::vector<uint8_t>(first, first + sizeof(first)));

    publisher.succeed = true;
    uint64_t publishedSequence = 0U;
    assert(outbox.publishNext(publishMessage, &publisher, &publishedSequence) ==
           OutboxResult::OK);
    assert(publishedSequence == firstSequence);
    assert(publisher.sequences.back() == firstSequence);
    assert(outbox.peek(pending) == OutboxResult::OK);
    assert(pending.sequence == secondSequence);
    assert(outbox.acknowledge(firstSequence) == OutboxResult::INVALID_ARGUMENT);

    TelemetryOutbox restarted(storage, "/outbox");
    assert(restarted.begin() == OutboxResult::OK);
    assert(restarted.peek(pending) == OutboxResult::OK);
    assert(pending.sequence == secondSequence);
    uint64_t thirdSequence = 0U;
    assert(restarted.enqueue("fleet/unit/telemetry", first, sizeof(first),
                             thirdSequence) == OutboxResult::OK);
    assert(thirdSequence == 3U);
}

void testStorageInitializationFailureIsReported()
{
    MemoryStorage storage;
    storage.failCreateDirectory = true;
    TelemetryOutbox outbox(storage, "/outbox");
    assert(outbox.begin() == OutboxResult::IO_ERROR);
}

void testCorruptMetadataSlotRecoversFromOtherSlotAndRecords()
{
    MemoryStorage storage;
    TelemetryOutbox outbox(storage, "/outbox");
    assert(outbox.begin() == OutboxResult::OK);
    const uint8_t payload[] = {'m'};
    uint64_t first = 0U;
    uint64_t second = 0U;
    assert(outbox.enqueue("fleet/unit/telemetry", payload, sizeof(payload),
                          first) == OutboxResult::OK);
    assert(outbox.enqueue("fleet/unit/gps", payload, sizeof(payload),
                          second) == OutboxResult::OK);
    assert(first == 1U && second == 2U);

    storage.files["/outbox/sequence_0.bin"].back() ^= 0x01U;
    TelemetryOutbox restarted(storage, "/outbox");
    assert(restarted.begin() == OutboxResult::OK);
    OutboxMessage pending;
    assert(restarted.peek(pending) == OutboxResult::OK);
    assert(pending.sequence == first);
    assert(pending.topic == "fleet/unit/telemetry");

    uint64_t third = 0U;
    assert(restarted.enqueue("fleet/unit/alarm", payload, sizeof(payload),
                             third) == OutboxResult::OK);
    assert(third == 3U);
}

void testReconnectReplaysBacklogInSequenceOrder()
{
    MemoryStorage storage;
    TelemetryOutbox outbox(storage, "/outbox");
    assert(outbox.begin() == OutboxResult::OK);

    const uint8_t one[] = {'1'};
    const uint8_t two[] = {'2'};
    uint64_t first = 0U;
    uint64_t second = 0U;
    assert(outbox.enqueue("fleet/unit/telemetry", one, sizeof(one), first) ==
           OutboxResult::OK);
    assert(outbox.enqueue("fleet/unit/gps", two, sizeof(two), second) ==
           OutboxResult::OK);

    PublishState publisher;
    assert(outbox.publishNext(publishMessage, &publisher) == OutboxResult::IO_ERROR);
    assert(outbox.publishNext(publishMessage, &publisher) == OutboxResult::IO_ERROR);
    publisher.succeed = true;
    assert(outbox.publishNext(publishMessage, &publisher) == OutboxResult::OK);
    assert(outbox.publishNext(publishMessage, &publisher) == OutboxResult::OK);
    assert((publisher.sequences ==
            std::vector<uint64_t>{first, first, first, second}));
    assert((publisher.topics == std::vector<std::string>{
        "fleet/unit/telemetry", "fleet/unit/telemetry",
        "fleet/unit/telemetry", "fleet/unit/gps"}));
    OutboxMessage empty;
    assert(outbox.peek(empty) == OutboxResult::EMPTY);
}

void testRestartPromotesCompletedInterruptedRecordWrite()
{
    MemoryStorage storage;
    TelemetryOutbox outbox(storage, "/outbox");
    assert(outbox.begin() == OutboxResult::OK);

    const uint8_t payload[] = {'r', 'e', 'c', 'o', 'v', 'e', 'r'};
    uint64_t sequence = 0U;
    assert(outbox.enqueue("fleet/unit/telemetry", payload, sizeof(payload),
                          sequence) == OutboxResult::OK);
    storage.files["/outbox/record_1.tmp"] =
        storage.files["/outbox/record_1.msg"];
    storage.files.erase("/outbox/record_1.msg");

    TelemetryOutbox restarted(storage, "/outbox");
    assert(restarted.begin() == OutboxResult::OK);
    OutboxMessage pending;
    assert(restarted.peek(pending) == OutboxResult::OK);
    assert(pending.sequence == sequence);
    assert(pending.topic == "fleet/unit/telemetry");
    assert(pending.payload ==
           std::vector<uint8_t>(payload, payload + sizeof(payload)));
}

void testSuccessfulPublishWithDeleteFailureRetainsRecord()
{
    MemoryStorage storage;
    TelemetryOutbox outbox(storage, "/outbox");
    assert(outbox.begin() == OutboxResult::OK);
    const uint8_t payload[] = {'p'};
    uint64_t sequence = 0U;
    assert(outbox.enqueue("fleet/unit/alarm", payload, sizeof(payload),
                          sequence) == OutboxResult::OK);

    PublishState publisher;
    publisher.succeed = true;
    storage.failRemove = true;
    assert(outbox.publishNext(publishMessage, &publisher) == OutboxResult::IO_ERROR);
    OutboxMessage pending;
    assert(outbox.peek(pending) == OutboxResult::OK);
    assert(pending.sequence == sequence);

    storage.failRemove = false;
    assert(outbox.publishNext(publishMessage, &publisher) == OutboxResult::OK);
    assert(outbox.peek(pending) == OutboxResult::EMPTY);
}

void testInterruptedWriteAndFullStoragePreserveCommittedRecords()
{
    MemoryStorage storage;
    TelemetryOutbox outbox(storage, "/outbox");
    assert(outbox.begin() == OutboxResult::OK);

    const uint8_t payload[] = {'s', 'a', 'f', 'e'};
    uint64_t sequence = 0U;
    assert(outbox.enqueue("fleet/unit/telemetry", payload, sizeof(payload),
                          sequence) == OutboxResult::OK);
    const size_t committedUsage = storage.usedBytes();
    storage.capacity = committedUsage + 20U;

    uint64_t rejectedSequence = 0U;
    assert(outbox.enqueue("fleet/unit/telemetry", payload, sizeof(payload),
                          rejectedSequence) ==
           OutboxResult::IO_ERROR);
    OutboxMessage pending;
    assert(outbox.peek(pending) == OutboxResult::OK);
    assert(pending.sequence == sequence);
    assert(pending.payload == std::vector<uint8_t>(payload, payload + sizeof(payload)));

    storage.capacity = static_cast<size_t>(-1);
    uint64_t laterSequence = 0U;
    assert(outbox.enqueue("fleet/unit/telemetry", payload, sizeof(payload),
                          laterSequence) == OutboxResult::OK);
    assert(laterSequence > sequence + 1U);
    assert(outbox.peek(pending) == OutboxResult::OK);
    assert(pending.sequence == sequence);
}

void testInterruptedTemporaryRecordIsDiscardedWithoutSequenceReuse()
{
    MemoryStorage storage;
    TelemetryOutbox outbox(storage, "/outbox");
    assert(outbox.begin() == OutboxResult::OK);

    const uint8_t payload[] = {'o', 'k'};
    uint64_t sequence = 0U;
    assert(outbox.enqueue("fleet/unit/rfid", payload, sizeof(payload),
                          sequence) == OutboxResult::OK);
    storage.files["/outbox/record_2.tmp"] = {0x54U, 0x4FU};

    TelemetryOutbox restarted(storage, "/outbox");
    assert(restarted.begin() == OutboxResult::OK);
    assert(storage.files.count("/outbox/record_2.tmp") == 0U);
    uint64_t laterSequence = 0U;
    assert(restarted.enqueue("fleet/unit/telemetry", payload, sizeof(payload),
                             laterSequence) ==
           OutboxResult::OK);
    assert(laterSequence == 3U);

    OutboxMessage pending;
    assert(restarted.peek(pending) == OutboxResult::OK);
    assert(pending.sequence == sequence);
}

void testCorruptCommittedRecordBlocksReplayAndIsNotDeleted()
{
    MemoryStorage storage;
    TelemetryOutbox outbox(storage, "/outbox");
    assert(outbox.begin() == OutboxResult::OK);

    const uint8_t payload[] = {'i', 'n', 't', 'a', 'c', 't'};
    uint64_t sequence = 0U;
    assert(outbox.enqueue("fleet/unit/telemetry", payload, sizeof(payload),
                          sequence) == OutboxResult::OK);
    auto& record = storage.files["/outbox/record_1.msg"];
    record.back() ^= 0x01U;

    TelemetryOutbox restarted(storage, "/outbox");
    assert(restarted.begin() == OutboxResult::CORRUPT);
    assert(storage.files.count("/outbox/record_1.msg") == 1U);
}

void testWirePayloadCarriesPersistentSequenceForReceiverDeduplication()
{
    const uint8_t payload[] = "{\"deviceId\":\"unit\",\"value\":1}";
    std::string wirePayload;
    assert(fortico::services::TelemetryWirePayload::withOutboxSequence(
        payload, sizeof(payload) - 1U, 42U, wirePayload));
    assert(wirePayload ==
           "{\"outboxSeq\":42,\"deviceId\":\"unit\",\"value\":1}");

    const uint8_t invalid[] = "not-json";
    assert(!fortico::services::TelemetryWirePayload::withOutboxSequence(
        invalid, sizeof(invalid) - 1U, 43U, wirePayload));
}

void testSinkRoutesAllChannelsThroughBoundedOrderedHandoff()
{
    FakeHandoff handoff(8U);
    MqttTelemetrySink sink(handoff, "unit");
    const char payload[] = "{\"value\":1}";
    assert(sink.begin());
    assert(sink.publish(payload, sizeof(payload) - 1U));
    assert(sink.publishChannel("rfid", payload, sizeof(payload) - 1U, true));
    assert(sink.publishChannel("engine", payload, sizeof(payload) - 1U, true));
    assert(sink.publishChannel("alarm", payload, sizeof(payload) - 1U, true));
    assert(sink.publishChannel("status", payload, sizeof(payload) - 1U));
    assert(sink.publishChannel("gps", payload, sizeof(payload) - 1U));
    assert(sink.publishChannel("fuel", payload, sizeof(payload) - 1U));
    assert(sink.publishChannel("battery", payload, sizeof(payload) - 1U));
    assert(!sink.publishChannel("rfid", payload, sizeof(payload) - 1U, true));
    assert((handoff.messages == std::vector<std::pair<std::string, std::string>>{
        {"fleet/unit/telemetry", payload},
        {"fleet/unit/rfid", payload},
        {"fleet/unit/engine", payload},
        {"fleet/unit/alarm", payload},
        {"fleet/unit/status", payload},
        {"fleet/unit/gps", payload},
        {"fleet/unit/fuel", payload},
        {"fleet/unit/battery", payload}}));
}

}  // namespace

int main()
{
    UNITY_BEGIN();
    RUN_TEST(testStorageInitializationFailureIsReported);
    RUN_TEST(testCorruptMetadataSlotRecoversFromOtherSlotAndRecords);
    RUN_TEST(testFifoReplayAcknowledgesOnlyAfterPublish);
    RUN_TEST(testReconnectReplaysBacklogInSequenceOrder);
    RUN_TEST(testRestartPromotesCompletedInterruptedRecordWrite);
    RUN_TEST(testSuccessfulPublishWithDeleteFailureRetainsRecord);
    RUN_TEST(testInterruptedWriteAndFullStoragePreserveCommittedRecords);
    RUN_TEST(testInterruptedTemporaryRecordIsDiscardedWithoutSequenceReuse);
    RUN_TEST(testCorruptCommittedRecordBlocksReplayAndIsNotDeleted);
    RUN_TEST(testWirePayloadCarriesPersistentSequenceForReceiverDeduplication);
    RUN_TEST(testSinkRoutesAllChannelsThroughBoundedOrderedHandoff);
    return UNITY_END();
}
