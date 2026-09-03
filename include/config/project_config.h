// include/config/project_config.h
#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

#define PROJECT_NAME            "Fortico"
#define FIRMWARE_VERSION        "1.0.0"

#define DEVICE_ID               "ESP32_001"
#define VEHICLE_ID              "VEH001"

#define TELEMETRY_INTERVAL_MS   5000
#define GPS_INTERVAL_MS         5000
#define FUEL_INTERVAL_MS        30000
#define BATTERY_INTERVAL_MS     30000
#define STATUS_INTERVAL_MS      60000

#define MAX_AUTHORIZED_BADGES   20

#endif // PROJECT_CONFIG_H