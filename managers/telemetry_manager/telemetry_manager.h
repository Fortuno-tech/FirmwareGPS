#ifndef TELEMETRY_MANAGER_H
#define TELEMETRY_MANAGER_H

#include <Arduino.h>

#include "temperature_service.h"
#include "time_service.h"

struct TelemetryGPSData
{
    float latitude;
    float longitude;

    float speed;
    float altitude;

    int satellites;

    bool valid;
};

struct FuelData
{
    float level;
    float liters;

    bool alarm;

    bool valid;
};

struct BatteryData
{
    float voltage;
    int percentage;

    bool valid;
};

struct RFIDData
{
    String driverId;

    bool authorized;

    bool valid;
};

struct EngineData
{
    bool relay;
    bool ignition;

    bool valid;
};

struct NetworkData
{
    int rssi;

    String operatorName;

    bool valid;
};

struct SDCardData
{
    bool available;
};

class TelemetryManager
{
public:

    TelemetryManager(
        TemperatureService& temperatureService,
        TimeService& timeService
    );

    void begin();

    void setGPS(
        const TelemetryGPSData& data
    );

    void setFuel(
        const FuelData& data
    );

    void setBattery(
        const BatteryData& data
    );

    void setRFID(
        const RFIDData& data
    );

    void setEngine(
        const EngineData& data
    );

    void setNetwork(
        const NetworkData& data
    );

    void setSDCard(
        const SDCardData& data
    );

    String buildJSON();

private:

    TemperatureService&
        _temperatureService;

    TimeService& _timeService;

    TelemetryGPSData _gps;

    FuelData _fuel;

    BatteryData _battery;

    RFIDData _rfid;

    EngineData _engine;

    NetworkData _network;

    SDCardData _sdcard;
};

#endif