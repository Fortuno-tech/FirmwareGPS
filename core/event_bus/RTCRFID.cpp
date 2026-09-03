#include <Arduino.h>

#include "rfid_service.h"
#include "time_service.h"

RFIDService rfidService;
TimeService timeService;

void setup()
{
    Serial.begin(115200);

    delay(1000);

    Serial.println();
    Serial.println("=================================");
    Serial.println("        FORTICO FIRMWARE");
    Serial.println("        RFID + RTC - STEP 4");
    Serial.println("=================================");

    // =============================================
    // RFID + PN532 + Authentication + RTC
    // =============================================

    if (rfidService.begin())
    {
        Serial.println();
        Serial.println(
            "RFID INITIALISATION : OK"
        );
    }
    else
    {
        Serial.println();
        Serial.println(
            "RFID INITIALISATION : FAILED"
        );
    }

    // =============================================
    // TEMPORAIRE : réglage du DS3231
    // =============================================

    if (timeService.begin())
    {
        RTCDateTime initialTime;

        initialTime.year = 2026;
        initialTime.month = 8;
        initialTime.day = 22;

        initialTime.hour = 14;
        initialTime.minute = 55;
        initialTime.second = 0;

        if (
            timeService.setDateTime(
                initialTime
            )
        )
        {
            Serial.println();
            Serial.println(
                "RTC : HEURE CONFIGUREE"
            );
        }
        else
        {
            Serial.println();
            Serial.println(
                "RTC : ERREUR CONFIGURATION"
            );
        }
    }
}

void loop()
{
    rfidService.update();

    delay(10);
}