#ifndef AUTHENTICATION_MANAGER_H
#define AUTHENTICATION_MANAGER_H

#include <Arduino.h>

struct AuthenticationResult
{
    String driverId;
    bool authorized;
};

class AuthenticationManager
{
public:

    AuthenticationManager();

    bool begin();

    AuthenticationResult authenticate(
        const String& uid
    );

private:

    struct RFIDCredential
    {
        const char* uid;
        const char* driverId;
    };

    static const RFIDCredential _credentials[];

    static const size_t _credentialCount;
};

#endif