#include "rfid_service.h"

#include <cstring>

RFIDService::RFIDService(
    ForticoDrivers::PN532Driver& driver
)
    : _driver(driver),
      _ready(false),
      _uidLength(0)
{
    memset(
        _lastUid,
        0,
        sizeof(_lastUid)
    );
}

bool RFIDService::begin()
{
    Serial.println();
    Serial.println(
        "================================="
    );
    Serial.println(
        "       RFID SERVICE"
    );
    Serial.println(
        "================================="
    );

    if (!_driver.begin())
    {
        Serial.println(
            "RFID SERVICE : FAILED"
        );

        _ready = false;
        return false;
    }

    _ready = true;

    Serial.println(
        "RFID SERVICE : READY"
    );

    return true;
}

bool RFIDService::update()
{
    if (!_ready)
    {
        return false;
    }

    uint8_t uid[7];
    uint8_t uidLength = 0;

    if (!_driver.readUID(
            uid,
            &uidLength,
            1000
        ))
    {
        return false;
    }

    if (uidLength == 0)
    {
        return false;
    }

    memset(
        _lastUid,
        0,
        sizeof(_lastUid)
    );

    memcpy(
        _lastUid,
        uid,
        uidLength
    );

    _uidLength = uidLength;

    Serial.print(
        "[RFID] UID : "
    );

    for (uint8_t i = 0; i < uidLength; ++i)
    {
        if (uid[i] < 0x10)
        {
            Serial.print('0');
        }

        Serial.print(
            uid[i],
            HEX
        );

        if (i + 1 < uidLength)
        {
            Serial.print(':');
        }
    }

    Serial.println();

    return true;
}

bool RFIDService::isReady() const
{
    return _ready;
}

String RFIDService::getLastUIDString() const
{
    if (_uidLength == 0)
    {
        return "";
    }

    String uidString = "";

    for (uint8_t i = 0; i < _uidLength; ++i)
    {
        if (_lastUid[i] < 0x10)
        {
            uidString += "0";
        }

        uidString += String(
            _lastUid[i],
            HEX
        );

        if (i + 1 < _uidLength)
        {
            uidString += ":";
        }
    }

    uidString = uidString.substring(0);
    uidString.toUpperCase();

    return uidString;
}

bool RFIDService::hasCard() const
{
    return _uidLength > 0;
}