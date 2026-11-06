#include "hal/flash/flash_hal.h"

#ifdef ESP32_TARGET
#include <esp_partition.h>
#endif

namespace fortico {
namespace hal {

#ifdef ESP32_TARGET
namespace {

constexpr const char* kPartitionLabel = "fw_firmware";
constexpr size_t kSectorSize = 4096U;

const esp_partition_t* getPartition() {
    return esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_ANY,
        kPartitionLabel);
}

bool isRangeValid(const esp_partition_t* partition, uint32_t address, size_t length) {
    return partition != nullptr && length != 0U && address <= partition->size && length <= partition->size - address;
}

}  // namespace
#endif

bool FlashHal::write(uint32_t address, const uint8_t* data, size_t length) {
#ifdef ESP32_TARGET
    const esp_partition_t* partition = getPartition();
    return data != nullptr && isRangeValid(partition, address, length) &&
        esp_partition_write(partition, address, data, length) == ESP_OK;
#else
    (void)address;
    (void)data;
    (void)length;
    return false;
#endif
}

bool FlashHal::read(uint32_t address, uint8_t* buffer, size_t length) {
#ifdef ESP32_TARGET
    const esp_partition_t* partition = getPartition();
    return buffer != nullptr && isRangeValid(partition, address, length) &&
        esp_partition_read(partition, address, buffer, length) == ESP_OK;
#else
    (void)address;
    (void)buffer;
    (void)length;
    return false;
#endif
}

bool FlashHal::erase(uint32_t sectorAddress) {
#ifdef ESP32_TARGET
    const esp_partition_t* partition = getPartition();
    return sectorAddress % kSectorSize == 0U && isRangeValid(partition, sectorAddress, kSectorSize) &&
        esp_partition_erase_range(partition, sectorAddress, kSectorSize) == ESP_OK;
#else
    (void)sectorAddress;
    return false;
#endif
}

}  // namespace hal
}  // namespace fortico
