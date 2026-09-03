#include "authentication_manager.h"


const AuthenticationManager::RFIDCredential
AuthenticationManager::_credentials[] =
{
     {
        "91:51:E2:06",
        "RFID00015"
    },
    
    {
        "FB:C2:1C:07",
        "RFID00016"
    }
};

const size_t
AuthenticationManager::_credentialCount =
    sizeof(
        AuthenticationManager::_credentials
    ) /
    sizeof(
        AuthenticationManager::_credentials[0]
    );

AuthenticationManager::AuthenticationManager()
{
}

bool AuthenticationManager::begin()
{
    Serial.println();
    Serial.println(
        "================================="
    );

    Serial.println(
        "   AUTHENTICATION MANAGER"
    );

    Serial.println(
        "================================="
    );

    Serial.print(
        "Badges locaux : "
    );

    Serial.println(
        _credentialCount
    );

    Serial.println(
        "Authentication Manager : OK"
    );

    return true;
}

AuthenticationResult
AuthenticationManager::authenticate(
    const String& uid
)
{
    AuthenticationResult result;

    result.driverId = "";
    result.authorized = false;

    for (
        size_t i = 0;
        i < _credentialCount;
        i++
    )
    {
        if (
            uid.equals(
                _credentials[i].uid
            )
        )
        {
            result.driverId =
                _credentials[i].driverId;

            result.authorized = true;

            return result;
        }
    }

    return result;
}