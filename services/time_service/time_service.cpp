#include "time_service.h"

TimeService::TimeService()
    : _ready(false)
{
}

bool TimeService::begin()
{
    Serial.println();
    Serial.println(
        "================================="
    );

    Serial.println(
        "         TIME SERVICE"
    );

    Serial.println(
        "================================="
    );

    if (!_rtc.begin())
    {
        Serial.println(
            "Time Service : FAILED"
        );

        _ready = false;

        return false;
    }

    _ready = true;

    Serial.println(
        "Time Service : OK"
    );

    return true;
}

bool TimeService::getDateTime(
    RTCDateTime& dateTime
)
{
    if (!_ready)
    {
        return false;
    }

    return _rtc.readDateTime(
        dateTime
    );
}

bool TimeService::isReady() const
{
    return _ready;
}

bool TimeService::setDateTime(
    const RTCDateTime& dateTime
)
{
    if (!_ready)
    {
        return false;
    }

    return _rtc.setDateTime(
        dateTime
    );
}

String TimeService::getISO8601()
{
    RTCDateTime dateTime;

    if (
        !getDateTime(
            dateTime
        )
    )
    {
        return "";
    }

    char timestamp[25];

    snprintf(
        timestamp,
        sizeof(timestamp),
        "%04d-%02d-%02dT%02d:%02d:%02dZ",
        dateTime.year,
        dateTime.month,
        dateTime.day,
        dateTime.hour,
        dateTime.minute,
        dateTime.second
    );

    return String(
        timestamp
    );
}