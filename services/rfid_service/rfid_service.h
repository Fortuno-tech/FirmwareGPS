#ifndef RFID_SERVICE_H
#define RFID_SERVICE_H

#include <Arduino.h>

#include "pn532_driver.h"
#include "authentication_manager.h"
#include "time_service.h"

class RFIDService
{
public:

    RFIDService();

    bool begin();

    void update();

private:

    ForticoDrivers::PN532Driver _driver;

    AuthenticationManager _authentication;

    // UID actuellement détecté
    uint8_t _uid[7];

    uint8_t _uidLength;
    TimeService _time;
    // Dernier UID traité
    uint8_t _lastUID[7];

    uint8_t _lastUIDLength;

    // État de présence de la carte
    bool _cardPresent;

    void handleCard();

    String formatUID(
        const uint8_t* uid,
        uint8_t uidLength
    ) const;

    bool isSameCard(
        const uint8_t* uid,
        uint8_t uidLength
    ) const;

    void rememberCard();

    void resetCard();

};

#endif