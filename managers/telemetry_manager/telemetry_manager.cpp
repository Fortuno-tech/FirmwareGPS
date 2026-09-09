#include "telemetry_manager.h"

#include <ArduinoJson.h>

#include "project_config.h"

TelemetryManager::TelemetryManager(
    TemperatureService& temperatureService,
    TimeService& timeService
)
    : _temperatureService(
          temperatureService
      ),
      _timeService(
          timeService
      )
{
    _gps = TelemetryGPSData{0.0f, 0.0f, 0.0f, 0.0f, 0, false};
    _fuel = FuelData{0.0f, 0.0f, false, false};
    _battery = BatteryData{0.0f, 0, false};
    _rfid = RFIDData{"", false, false};
    _engine = EngineData{false, false, false};
    _network = NetworkData{0, "", false};
    _sdcard = SDCardData{false};
}

void TelemetryManager::begin()
{
    Serial.println(
        "[TELEMETRY] Manager initialisé"
    );
}

void TelemetryManager::setGPS(
    const TelemetryGPSData& data
)
{
    _gps = data;
}

void TelemetryManager::setFuel(
    const FuelData& data
)
{
    _fuel = data;
}

void TelemetryManager::setBattery(
    const BatteryData& data
)
{
    _battery = data;
}

void TelemetryManager::setRFID(
    const RFIDData& data
)
{
    _rfid = data;
}

void TelemetryManager::setEngine(
    const EngineData& data
)
{
    _engine = data;
}

void TelemetryManager::setNetwork(
    const NetworkData& data
)
{
    _network = data;
}

void TelemetryManager::setSDCard(
    const SDCardData& data
)
{
    _sdcard = data;
}

String TelemetryManager::buildJSON()
{
    JsonDocument doc;

    JsonObject root = doc.to<JsonObject>();

    root["deviceId"] = DEVICE_ID;
    root["vehicleId"] = VEHICLE_ID;
    root["firmware"] = FIRMWARE_VERSION;

    String isoTimestamp = _timeService.getISO8601();
    root["timestamp"] = isoTimestamp.length() > 0 ? isoTimestamp : "";

    JsonObject gps = root.createNestedObject("gps");
    gps["latitude"] = _gps.valid ? _gps.latitude : 0.0f;
    gps["longitude"] = _gps.valid ? _gps.longitude : 0.0f;
    gps["speed"] = _gps.valid ? _gps.speed : 0.0f;
    gps["altitude"] = _gps.valid ? _gps.altitude : 0.0f;
    gps["satellites"] = _gps.valid ? _gps.satellites : 0;
    gps["valid"] = _gps.valid;

    JsonObject fuel = root.createNestedObject("fuel");
    fuel["level"] = _fuel.valid ? _fuel.level : 0.0f;
    fuel["liters"] = _fuel.valid ? _fuel.liters : 0.0f;
    fuel["alarm"] = _fuel.valid ? _fuel.alarm : false;
    fuel["valid"] = _fuel.valid;

    JsonObject battery = root.createNestedObject("battery");
    battery["voltage"] = _battery.valid ? _battery.voltage : 0.0f;
    battery["percentage"] = _battery.valid ? _battery.percentage : 0;
    battery["valid"] = _battery.valid;

    JsonObject rfid = root.createNestedObject("rfid");
    rfid["driverId"] = _rfid.valid ? _rfid.driverId : "";
    rfid["authorized"] = _rfid.valid ? _rfid.authorized : false;
    rfid["valid"] = _rfid.valid;

    JsonObject engine = root.createNestedObject("engine");
    engine["relay"] = _engine.valid ? _engine.relay : false;
    engine["ignition"] = _engine.valid ? _engine.ignition : false;
    engine["valid"] = _engine.valid;

    JsonObject network = root.createNestedObject("network");
    network["rssi"] = _network.valid ? _network.rssi : 0;
    network["operator"] = _network.valid ? _network.operatorName : "";
    network["registered"] = _network.valid;
    network["internet"] = _network.valid;
    network["valid"] = _network.valid;

    JsonObject temperature = root.createNestedObject("temperature");
    if (_temperatureService.isValid())
    {
        temperature["value"] = _temperatureService.getTemperature();
        temperature["humidity"] = _temperatureService.getHumidity();
        temperature["unit"] = "C";
        temperature["valid"] = true;
    }
    else
    {
        temperature["value"] = 0.0f;
        temperature["humidity"] = 0.0f;
        temperature["unit"] = "C";
        temperature["valid"] = false;
    }

    JsonObject sdcard = root.createNestedObject("sdcard");
    sdcard["available"] = _sdcard.available;

    String output;
    serializeJson(doc, output);

    return output;
}