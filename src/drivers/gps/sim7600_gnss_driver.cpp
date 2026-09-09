#include "drivers/gps/sim7600_gnss_driver.h"

#include <cstdlib>
#include <cstring>

#include "config/hardware_config.h"
#include "utils/default_values.h"
#include "utils/timer_utils.h"

namespace fortico {
namespace drivers {
namespace {

constexpr std::size_t kResponseSize = 256U;
constexpr std::size_t kFieldCount = 16U;
constexpr std::size_t kFieldSize = 24U;
constexpr float kKnotsToKmh = 1.852F;

types::GpsData invalidPosition()
{
    return {0.0, 0.0, constants::INVALID_ALTITUDE, constants::INVALID_SPEED,
            0.0F, constants::INVALID_DOP, constants::INVALID_DOP,
            constants::INVALID_SATELLITE_COUNT, 0U, false,
            utils::Timer::millis()};
}

bool parseUnsigned(const char* value, uint8_t& result)
{
    if (value == nullptr || value[0] == '\0') return false;
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(value, &end, 10);
    if (end == value || *end != '\0' || parsed > 255U) return false;
    result = static_cast<uint8_t>(parsed);
    return true;
}

bool parseFloat(const char* value, float& result)
{
    if (value == nullptr || value[0] == '\0') return false;
    char* end = nullptr;
    const float parsed = std::strtof(value, &end);
    if (end == value || *end != '\0') return false;
    result = parsed;
    return true;
}

bool parseCoordinate(const char* value, char hemisphere, double& result)
{
    if (value == nullptr || value[0] == '\0' ||
        (hemisphere != 'N' && hemisphere != 'S' &&
         hemisphere != 'E' && hemisphere != 'W')) {
        return false;
    }
    char* end = nullptr;
    const double encoded = std::strtod(value, &end);
    if (end == value || *end != '\0' || encoded < 0.0) return false;
    const int degrees = static_cast<int>(encoded / 100.0);
    const double minutes = encoded - static_cast<double>(degrees * 100);
    if (minutes < 0.0 || minutes >= 60.0) return false;
    result = static_cast<double>(degrees) + minutes / 60.0;
    if (hemisphere == 'S' || hemisphere == 'W') result = -result;
    return true;
}

}  // namespace

Sim7600GnssDriver::Sim7600GnssDriver(Sim7600Driver& modem)
    : _modem(modem), _initialized(false), _healthy(false), _active(false),
      _gnssEnabled(false)
{
}

constants::ErrorCode Sim7600GnssDriver::init()
{
    if (!_modem.isHealthy()) return constants::ErrorCode::MODEM_AT_TIMEOUT;
    _initialized = true;
    _healthy = true;
    _active = true;
    return constants::ErrorCode::OK;
}

constants::ErrorCode Sim7600GnssDriver::poll()
{
    if (!_initialized) return constants::ErrorCode::NOT_INITIALIZED;
    if (!_active) return constants::ErrorCode::OK;
    if (!_modem.isHealthy()) {
        _healthy = false;
        return constants::ErrorCode::MODEM_AT_TIMEOUT;
    }
    return _healthy ? constants::ErrorCode::OK : constants::ErrorCode::GNSS_TIMEOUT;
}

bool Sim7600GnssDriver::start()
{
    if (!_initialized || !_modem.isHealthy()) return false;
    _active = true;
    _healthy = true;
    return true;
}

bool Sim7600GnssDriver::stop()
{
    if (!_initialized) return false;
    disableGnss();
    _active = false;
    return true;
}

bool Sim7600GnssDriver::isHealthy() const
{
    return _initialized && _active && _healthy && _modem.isHealthy();
}

bool Sim7600GnssDriver::isInitialized() const { return _initialized; }

void Sim7600GnssDriver::reset()
{
    _healthy = _modem.isHealthy();
    _active = _healthy;
    _gnssEnabled = false;
}

bool Sim7600GnssDriver::enableGnss()
{
    if (!isHealthy()) return false;
    if (isGnssEnabled()) {
        _gnssEnabled = true;
        return true;
    }
    char response[96]{};
    _gnssEnabled = sendCommand("AT+CGPS=1", response, sizeof(response),
                               hardware::SIM7600_AT_TIMEOUT_LONG_MS);
    _healthy = _gnssEnabled;
    return _gnssEnabled;
}

bool Sim7600GnssDriver::disableGnss()
{
    if (!_initialized || !_active || !_gnssEnabled) return true;
    char response[96]{};
    const bool disabled = sendCommand("AT+CGPS=0", response, sizeof(response),
                                      hardware::SIM7600_AT_TIMEOUT_LONG_MS);
    if (disabled) _gnssEnabled = false;
    return disabled;
}

bool Sim7600GnssDriver::readPosition(types::GpsData& position)
{
    position = invalidPosition();
    if (!isHealthy() || !_gnssEnabled) return false;
    char response[kResponseSize]{};
    if (!sendCommand("AT+CGNSSINFO", response, sizeof(response),
                     hardware::SIM7600_GNSS_QUERY_TIMEOUT_MS)) {
        _healthy = false;
        return false;
    }
    return parseCgnssInfo(response, position);
}

bool Sim7600GnssDriver::isGnssEnabled()
{
    char response[96]{};
    return sendCommand("AT+CGPS?", response, sizeof(response),
                       hardware::SIM7600_AT_TIMEOUT_LONG_MS) &&
           std::strstr(response, "+CGPS: 1") != nullptr;
}

bool Sim7600GnssDriver::sendCommand(const char* command, char* response,
                                    std::size_t responseSize, uint32_t timeoutMs)
{
    return _modem.sendCommand(command, response, responseSize, timeoutMs);
}

bool Sim7600GnssDriver::parseCgnssInfo(const char* response,
                                       types::GpsData& position) const
{
    const char* prefix = std::strstr(response, "+CGNSSINFO:");
    if (prefix == nullptr) return false;
    const char* cursor = std::strchr(prefix, ':');
    if (cursor == nullptr) return false;
    ++cursor;
    while (*cursor == ' ') ++cursor;

    char fields[kFieldCount][kFieldSize]{};
    std::size_t field = 0U;
    std::size_t character = 0U;
    for (; *cursor != '\0' && *cursor != '\r' && *cursor != '\n'; ++cursor) {
        if (*cursor == ',') {
            if (field + 1U >= kFieldCount) return false;
            fields[field][character] = '\0';
            ++field;
            character = 0U;
            continue;
        }
        if (character + 1U >= kFieldSize) return false;
        fields[field][character++] = *cursor;
    }
    fields[field][character] = '\0';
    if (field + 1U != kFieldCount) return false;

    uint8_t fixMode = 0U;
    uint8_t gpsSatellites = 0U;
    uint8_t glonassSatellites = 0U;
    uint8_t beidouSatellites = 0U;
    if (!parseUnsigned(fields[0], fixMode) ||
        !parseUnsigned(fields[1], gpsSatellites) ||
        !parseUnsigned(fields[2], glonassSatellites) ||
        !parseUnsigned(fields[3], beidouSatellites)) {
        return false;
    }

    double latitude = 0.0;
    double longitude = 0.0;
    if (!parseCoordinate(fields[4], fields[5][0], latitude) ||
        !parseCoordinate(fields[6], fields[7][0], longitude)) {
        return false;
    }

    float altitude = constants::INVALID_ALTITUDE;
    float speedKnots = 0.0F;
    float heading = 0.0F;
    float pdop = constants::INVALID_DOP;
    float hdop = constants::INVALID_DOP;
    if (!parseFloat(fields[10], altitude) || !parseFloat(fields[11], speedKnots) ||
        !parseFloat(fields[12], heading) || !parseFloat(fields[13], pdop) ||
        !parseFloat(fields[14], hdop)) {
        return false;
    }

    const uint16_t totalSatellites = static_cast<uint16_t>(gpsSatellites) +
        static_cast<uint16_t>(glonassSatellites) +
        static_cast<uint16_t>(beidouSatellites);
    if (totalSatellites > 255U) return false;

    position.latitude = latitude;
    position.longitude = longitude;
    position.altitudeMeters = altitude;
    position.speedKmh = speedKnots * kKnotsToKmh;
    position.headingDeg = heading;
    position.pdop = pdop;
    position.hdop = hdop;
    position.satellites = static_cast<uint8_t>(totalSatellites);
    position.fixMode = fixMode;
    position.fixValid = (fixMode == 2U || fixMode == 3U) &&
        position.satellites >= hardware::GPS_MIN_SATELLITES_FOR_FIX;
    if (!position.fixValid) position.speedKmh = constants::INVALID_SPEED;
    position.timestamp = utils::Timer::millis();
    return position.fixValid;
}

}  // namespace drivers
}  // namespace fortico
