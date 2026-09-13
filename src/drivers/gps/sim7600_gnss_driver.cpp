#include "drivers/gps/sim7600_gnss_driver.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#ifdef ESP32_TARGET
#include <Arduino.h>
#endif

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
            constants::INVALID_SATELLITE_COUNT, 0U, false, false,
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

Sim7600GnssDriver::Sim7600GnssDriver(Sim7600Driver& modem, core::Logger& logger)
    : _modem(modem), _logger(logger), _initialized(false), _healthy(false), _active(false),
    _gnssEnabled(false), _hasReportedFixState(false),
    _lastReportedFixValid(false)
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
#ifdef ESP32_TARGET
    _logger.info(logging::TAG_GPS, "GNSS %s", _gnssEnabled ? "enabled" : "disabled");
#endif
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
    const uint32_t startMs = utils::Timer::millis();
    const bool commandOk = sendCommand("AT+CGNSSINFO", response, sizeof(response),
                                       hardware::SIM7600_GNSS_QUERY_TIMEOUT_MS);
    const Sim7600Driver::CommandFailure commandFailure = _modem.lastCommandFailure();
    const uint32_t durationMs = utils::Timer::millis() - startMs;
#ifdef ESP32_TARGET
    if (response[0] != '\0') _logger.debug(logging::TAG_GPS, "RAW %s", response);
#endif
    if (!commandOk) {
#ifdef ESP32_TARGET
        const char* reason = "unknown";
        switch (commandFailure) {
        case Sim7600Driver::CommandFailure::MUTEX_TIMEOUT: reason = "mutex timeout"; break;
        case Sim7600Driver::CommandFailure::UART_WRITE: reason = "UART failure"; break;
        case Sim7600Driver::CommandFailure::ERROR_RESPONSE: reason = "ERROR"; break;
        case Sim7600Driver::CommandFailure::TIMEOUT: reason = "timeout"; break;
        default: break;
        }
        if (commandFailure == Sim7600Driver::CommandFailure::MUTEX_TIMEOUT) {
            _logger.warn(logging::TAG_GPS, "SIM7600 mutex unavailable wait_ms=%u",
                         hardware::SIM7600_UART_MUTEX_TIMEOUT_MS);
        } else {
            _logger.warn(logging::TAG_GPS, "AT+CGNSSINFO failed cause=%s", reason);
        }
        _logger.warn(logging::TAG_GPS, "CGNSSINFO duration=%lums result=fail",
                     static_cast<unsigned long>(durationMs));
#endif
        // Une lecture GNSS peut expirer sans que le modem soit en panne.
        // poll() conserve l'etat de sante base sur la liaison modem.
        return false;
    }
    const char* failureReason = "unknown";
    const bool parsed = parseCgnssInfo(response, position, failureReason);
#ifdef ESP32_TARGET
    _logger.debug(logging::TAG_GPS, "CGNSSINFO duration=%lums result=success",
                  static_cast<unsigned long>(durationMs));
    if (!parsed) {
        _logger.debug(logging::TAG_GPS, "CGNSSINFO parse failed reason=%s", failureReason);
    } else if (!_hasReportedFixState || _lastReportedFixValid != position.fixValid) {
        _logger.info(logging::TAG_GPS, "%s fix mode=%u satellites=%u HDOP=%.2f PDOP=%.2f",
                     position.fixValid ? "valid" : "no", position.fixMode,
                     position.satellites, position.hdop, position.pdop);
        _hasReportedFixState = true;
        _lastReportedFixValid = position.fixValid;
    }
#endif
    return parsed;
}

bool Sim7600GnssDriver::isGnssEnabled()
{
    char response[96]{};
    const bool commandOk = sendCommand("AT+CGPS?", response, sizeof(response),
                                       hardware::SIM7600_AT_TIMEOUT_LONG_MS);
    const bool enabled = commandOk && std::strstr(response, "+CGPS: 1") != nullptr;
#ifdef ESP32_TARGET
    if (!commandOk) _logger.warn(logging::TAG_GPS, "AT+CGPS? failed");
    else _logger.debug(logging::TAG_GPS, "GNSS %s", enabled ? "enabled" : "disabled");
#endif
    return enabled;
}

bool Sim7600GnssDriver::sendCommand(const char* command, char* response,
                                    std::size_t responseSize, uint32_t timeoutMs)
{
    return _modem.sendCommand(command, response, responseSize, timeoutMs);
}

bool Sim7600GnssDriver::parseCgnssInfo(const char* response,
                                       types::GpsData& position,
                                       const char*& failureReason) const
{
    const auto reject = [&failureReason](const char* reason) {
        failureReason = reason;
        return false;
    };
    const char* prefix = std::strstr(response, "+CGNSSINFO:");
    if (prefix == nullptr) return reject("unexpected format");
    const char* cursor = std::strchr(prefix, ':');
    if (cursor == nullptr) return reject("unexpected format");
    ++cursor;
    while (*cursor == ' ') ++cursor;

    char fields[kFieldCount][kFieldSize]{};
    std::size_t field = 0U;
    std::size_t character = 0U;
    for (; *cursor != '\0' && *cursor != '\r' && *cursor != '\n'; ++cursor) {
        if (*cursor == ',') {
            if (field + 1U >= kFieldCount) return reject("field_count");
            fields[field][character] = '\0';
            ++field;
            character = 0U;
            continue;
        }
        if (character + 1U >= kFieldSize) return reject("field too long");
        fields[field][character++] = *cursor;
    }
    fields[field][character] = '\0';
    if (field + 1U != kFieldCount) return reject("field_count");

    uint8_t fixMode = 0U;
    uint8_t gpsSatellites = 0U;
    uint8_t glonassSatellites = 0U;
    uint8_t beidouSatellites = 0U;
    if (!parseUnsigned(fields[0], fixMode) ||
        !parseUnsigned(fields[1], gpsSatellites) ||
        !parseUnsigned(fields[2], glonassSatellites) ||
        !parseUnsigned(fields[3], beidouSatellites)) {
        return reject("invalid numeric field");
    }

    double latitude = 0.0;
    double longitude = 0.0;
    if (!parseCoordinate(fields[4], fields[5][0], latitude) ||
        !parseCoordinate(fields[6], fields[7][0], longitude)) {
        return reject("invalid latitude/longitude");
    }
    if (!std::isfinite(latitude) || !std::isfinite(longitude) ||
        latitude < -90.0 || latitude > 90.0 ||
        longitude < -180.0 || longitude > 180.0) {
        return reject("invalid latitude/longitude");
    }

    float altitude = constants::INVALID_ALTITUDE;
    float speedKnots = 0.0F;
    float heading = constants::INVALID_DOP;
    float pdop = constants::INVALID_DOP;
    float hdop = constants::INVALID_DOP;
    if (!parseFloat(fields[10], altitude) || !parseFloat(fields[11], speedKnots)) {
        return reject("invalid numeric field");
    }
    if (fields[12][0] == '\0') {
        heading = constants::INVALID_DOP;
    } else if (!parseFloat(fields[12], heading)) {
        return reject("invalid numeric field");
    }
    if (!parseFloat(fields[13], pdop) || !parseFloat(fields[14], hdop)) {
        return reject("invalid numeric field");
    }
    if (!std::isfinite(altitude) || !std::isfinite(speedKnots) ||
        !std::isfinite(heading) || !std::isfinite(pdop) ||
        !std::isfinite(hdop)) {
        return reject("invalid numeric field");
    }

    const uint16_t totalSatellites = static_cast<uint16_t>(gpsSatellites) +
        static_cast<uint16_t>(glonassSatellites) +
        static_cast<uint16_t>(beidouSatellites);
    if (totalSatellites > 255U) return reject("invalid numeric field");

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
