#include <Arduino.h>

#include "rfid_service.h"

ForticoDrivers::PN532Driver pn532Driver;
RFIDService rfidService(pn532Driver);

void setup()
{
    Serial.begin(115200);

    delay(1000);

    Serial.println();
    Serial.println("=================================");
    Serial.println("        FROTICO FIRMWARE");
    Serial.println("        RFID TEST - STEP 1");
    Serial.println("=================================");

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
}

void loop()
{
    rfidService.update();

    delay(10);
}