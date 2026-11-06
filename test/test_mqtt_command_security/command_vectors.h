#pragma once

#include <cstdint>

namespace command_vectors {

constexpr char deviceId[] = "DEVICE-1";
constexpr char testKeyHex[] =
    "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";

constexpr char validId[] = "cmd-valid-001";
constexpr char validCommand[] = "STOP";
constexpr uint64_t validSequence = 1U;
constexpr uint64_t validTimestamp = 1000U;
constexpr char validCanonical[] =
    "FORTICO-CMD-V1\nDEVICE-1\ncmd-valid-001\n1\nSTOP\n1000";
constexpr char validMacHex[] =
    "1c84b54cfae68b9b598d1e5dc6d777b1cf1fe0c11674b23d9a81913e101f04f5";
constexpr char validJson[] =
    R"({"id":"cmd-valid-001","seq":1,"command":"STOP","timestamp":1000,"mac":"1c84b54cfae68b9b598d1e5dc6d777b1cf1fe0c11674b23d9a81913e101f04f5"})";

constexpr char expiredId[] = "cmd-expired-001";
constexpr char expiredCommand[] = "STOP";
constexpr uint64_t expiredSequence = 2U;
constexpr uint64_t expiredTimestamp = 900U;
constexpr char expiredCanonical[] =
    "FORTICO-CMD-V1\nDEVICE-1\ncmd-expired-001\n2\nSTOP\n900";
constexpr char expiredMacHex[] =
    "0c7e0dc999d1a3364182f2c789d9d43a0decadb76957a7de8dc87cf0b1002138";
constexpr char expiredJson[] =
    R"({"id":"cmd-expired-001","seq":2,"command":"STOP","timestamp":900,"mac":"0c7e0dc999d1a3364182f2c789d9d43a0decadb76957a7de8dc87cf0b1002138"})";

}  // namespace command_vectors
