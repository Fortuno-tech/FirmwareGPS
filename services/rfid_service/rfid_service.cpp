#include "rfid_service.h"

#include "i2c_hal.h"

RFIDService::RFIDService()
    : _uidLength(0),
      _lastUIDLength(0),
      _cardPresent(false)
{
    memset(
        _uid,
        0,
        sizeof(_uid)
    );

    memset(
        _lastUID,
        0,
        sizeof(_lastUID)
    );
}

bool RFIDService::begin()
{
    Serial.println();
    Serial.println(
        "================================="
    );

    Serial.println(
        "         RFID SERVICE"
    );

    Serial.println(
        "================================="
    );

    // =============================================
    // I2C
    // =============================================

    if (!ForticoHAL::I2CHAL::init())
    {
        Serial.println(
            "ERREUR : I2C non initialise."
        );

        return false;
    }

    Serial.println(
        "I2C : OK"
    );

    // =============================================
    // PN532
    // =============================================

    if (!_driver.begin())
    {
        Serial.println(
            "ERREUR : PN532 indisponible."
        );

        return false;
    }

    // =============================================
    // Authentication Manager
    // =============================================

    if (!_authentication.begin())
    {
        Serial.println(
            "ERREUR : Authentication Manager."
        );

        return false;
    }

   if (!_time.begin())
   {
    Serial.println(
        "ERREUR : Time Service."
    );

        return false;
    }

    Serial.println(
        "RFID Service : OK"
    );

    Serial.println();
    Serial.println(
        "Approchez une carte RFID..."
    );

    return true;
}

void RFIDService::update()
{
    if (!_driver.isReady())
    {
        return;
    }

    bool success =
        _driver.readUID(
            _uid,
            &_uidLength,
            50
        );

    if (!success)
    {
        /*
         * Aucune carte détectée.
         *
         * Si une carte était précédemment
         * présente, on considère maintenant
         * qu'elle a été retirée.
         */

        if (_cardPresent)
        {
            resetCard();
        }

        return;
    }

    // =============================================
    // Vérifier si c'est la même carte
    // =============================================

    if (
        _cardPresent &&
        isSameCard(
            _uid,
            _uidLength
        )
    )
    {
        /*
         * Même carte toujours présente.
         *
         * On ne génère PAS un nouvel événement.
         */

        return;
    }

    // =============================================
    // Nouvelle carte
    // =============================================

    handleCard();

    // Mémoriser cette carte
    rememberCard();
}

void RFIDService::handleCard()
{
    String uid =
        formatUID(
            _uid,
            _uidLength
        );

    Serial.println();
    Serial.println(
        "================================="
    );

    Serial.println(
        "       NOUVEAU SCAN RFID"
    );

    Serial.println(
        "================================="
    );

    Serial.print(
        "UID : "
    );

    Serial.println(
        uid
    );

    Serial.print(
        "Longueur : "
    );

    Serial.print(
        _uidLength
    );

    Serial.println(
        " octets"
    );

    // =============================================
    // Authentification
    // =============================================

    AuthenticationResult result =
        _authentication.authenticate(
            uid
        );
    String timestamp =
        _time.getISO8601();

    Serial.println();

    Serial.println(
        "----- AUTHENTIFICATION -----"
    );

    Serial.print(
        "Driver ID : "
    );

    if (result.authorized)
    {
        Serial.println(
            result.driverId
        );
    }
    else
    {
        Serial.println(
            "UNKNOWN"
        );
    }

    Serial.print(
        "Authorized : "
    );

    if (result.authorized)
    {
        Serial.println(
            "true"
        );
    }
    else
    {
        Serial.println(
            "false"
        );
    }

    Serial.print(
    "Timestamp : "
);

if (timestamp.length() > 0)
{
    Serial.println(
        timestamp
    );
}
else
{
    Serial.println(
        "RTC ERROR"
    );
}

    Serial.println(
        "================================="
    );
}

bool RFIDService::isSameCard(
    const uint8_t* uid,
    uint8_t uidLength
) const
{
    if (
        uidLength !=
        _lastUIDLength
    )
    {
        return false;
    }

    for (
        uint8_t i = 0;
        i < uidLength;
        i++
    )
    {
        if (
            uid[i] !=
            _lastUID[i]
        )
        {
            return false;
        }
    }

    return true;
}

void RFIDService::rememberCard()
{
    memcpy(
        _lastUID,
        _uid,
        _uidLength
    );

    _lastUIDLength =
        _uidLength;

    _cardPresent = true;
}

void RFIDService::resetCard()
{
    Serial.println();
    Serial.println(
        "[RFID] Carte retiree."
    );

    memset(
        _lastUID,
        0,
        sizeof(_lastUID)
    );

    _lastUIDLength = 0;

    _cardPresent = false;
}

String RFIDService::formatUID(
    const uint8_t* uid,
    uint8_t uidLength
) const
{
    String result;

    for (
        uint8_t i = 0;
        i < uidLength;
        i++
    )
    {
        if (uid[i] < 0x10)
        {
            result += "0";
        }

        result += String(
            uid[i],
            HEX
        );

        if (
            i < uidLength - 1
        )
        {
            result += ":";
        }
    }

    result.toUpperCase();

    return result;
}