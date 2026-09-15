#pragma once

namespace fortico {
namespace mqtt_identity {

// Stable identifier assigned by the fleet backend, not the ESP32 hardware id.
constexpr const char* VEHICLE_ID = "VEH001";
constexpr const char* DEVICE_ID_PREFIX = "EV_DS6";

}  // namespace mqtt_identity
}  // namespace fortico