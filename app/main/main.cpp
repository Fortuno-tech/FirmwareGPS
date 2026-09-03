#include <Arduino.h>

#include "sim7600_driver.h"

SIM7600Driver sim7600;

void setup()
{
    Serial.begin(115200);

    delay(1000);

    Serial.println();
    Serial.println(
        "================================="
    );

    Serial.println(
        "       FORTICO FIRMWARE"
    );

    Serial.println(
        "       SIM7600 TEST - STEP 5"
    );

    Serial.println(
        "================================="
    );

    if (
        sim7600.begin()
    )
    {
        Serial.println();
        Serial.println(
            "SIM7600 INITIALISATION : OK"
        );

        String info =
            sim7600.sendCommand(
                "ATI",
                5000
            );

        Serial.println();
        Serial.println(
            "----- MODEM INFORMATION -----"
        );

        Serial.println(
            info
        );

        Serial.println(
            "================================="
        );
    }
    else
    {
        Serial.println();
        Serial.println(
            "SIM7600 INITIALISATION : FAILED"
        );
    }
}

void loop()
{
    delay(1000);
}