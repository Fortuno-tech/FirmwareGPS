#pragma once

#include "interfaces/isd_driver.h"

namespace fortico {
namespace storage {

// Root directory for all persisted Fortico files on the SD card.
static constexpr const char* kRootDir = "/fortico";

// Log stream: textual runtime log lines.
static constexpr const char* kLogDir = "/fortico/log";
static constexpr const char* kTextLogPath = "/fortico/log/fortico.log";

// Structured event log: JSONL entries.
static constexpr const char* kJsonlLogPath = "/fortico/log/fortico.jsonl";

// Error records, network diagnostics, RFID events, GPS history, etc.
static constexpr const char* kErrorDir = "/fortico/erreur";
static constexpr const char* kNetworkDir = "/fortico/reseau";
static constexpr const char* kRfidDir = "/fortico/rfid";
static constexpr const char* kGpsDir = "/fortico/gps";
static constexpr const char* kMqttDir = "/fortico/mqtt";
static constexpr const char* kSystemDir = "/fortico/systeme";

inline bool ensureLogDirectories(interfaces::ISdDriver& driver) {
    if (!driver.exists(kRootDir) && !driver.createDirectory(kRootDir)) {
        return false;
    }
    if (!driver.exists(kLogDir) && !driver.createDirectory(kLogDir)) {
        return false;
    }
    return true;
}

}  // namespace storage
}  // namespace fortico
