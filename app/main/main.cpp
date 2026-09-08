#include <Arduino.h>

#include "pin_config.h"

#include "sim7600_driver.h"
#include "sim7600_service.h"


// ============================================================
// OBJETS
// ============================================================

SIM7600Driver sim7600Driver;

SIM7600Service sim7600Service(
    sim7600Driver
);


// ============================================================
// POWER ON SIM7600
// ============================================================

void powerOnSim7600()
{
    Serial.println();
    Serial.println(
        "Allumage physique du SIM7600 (PWRKEY)..."
    );

    pinMode(
        SIM7600_PWRKEY_PIN,
        OUTPUT
    );

    // Etat initial
    digitalWrite(
        SIM7600_PWRKEY_PIN,
        HIGH
    );

    delay(100);

    // Impulsion PWRKEY
    digitalWrite(
        SIM7600_PWRKEY_PIN,
        LOW
    );

    delay(1500);

    // Relâchement
    digitalWrite(
        SIM7600_PWRKEY_PIN,
        HIGH
    );

    Serial.println(
        "PWRKEY termine."
    );

    Serial.println(
        "Attente demarrage du modem..."
    );

    delay(5000);
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
    // --------------------------------------------------------
    // DEBUG UART
    // --------------------------------------------------------

    Serial.begin(
        115200
    );

    delay(1000);


    // --------------------------------------------------------
    // HEADER
    // --------------------------------------------------------

    Serial.println();
    Serial.println(
        "================================="
    );

    Serial.println(
        "       FORTICO FIRMWARE"
    );

    Serial.println(
        "       SIM7600 - STEP 8"
    );

    Serial.println(
        "================================="
    );


    // --------------------------------------------------------
    // POWER ON
    // --------------------------------------------------------

    powerOnSim7600();


    // --------------------------------------------------------
    // SERVICE BEGIN
    // --------------------------------------------------------

    if (
        !sim7600Service.begin()
    )
    {
        Serial.println();
        Serial.println(
            "SIM7600 : INITIALISATION FAILED"
        );

        Serial.println(
            "Verification UART / alimentation / modem."
        );

        return;
    }


    // --------------------------------------------------------
    // ATTENTE RESEAU
    // --------------------------------------------------------

    Serial.println();
    Serial.println(
        "================================="
    );

    Serial.println(
        "     ATTENTE RESEAU MOBILE"
    );

    Serial.println(
        "================================="
    );

    if (
        !sim7600Service.waitForNetwork()
    )
    {
        Serial.println();
        Serial.println(
            "================================="
        );

        Serial.println(
            "     RESEAU NON DISPONIBLE"
        );

        Serial.println(
            "================================="
        );

        Serial.println();
        Serial.println(
            "Verifier :"
        );

        Serial.println(
            "1. Antenne LTE"
        );

        Serial.println(
            "2. Alimentation SIM7600"
        );

        Serial.println(
            "3. Carte SIM"
        );

        Serial.println(
            "4. Couverture reseau"
        );

        Serial.println(
            "5. Activation Internet"
        );

        return;
    }


    // --------------------------------------------------------
    // DIAGNOSTIC
    // --------------------------------------------------------

    Serial.println();
    Serial.println(
        "================================="
    );

    Serial.println(
        "     DIAGNOSTIC APRES RESEAU"
    );

    Serial.println(
        "================================="
    );

    if (
        !sim7600Service.runDiagnostics()
    )
    {
        Serial.println();
        Serial.println(
            "DIAGNOSTIC : FAILED"
        );

        return;
    }


    // --------------------------------------------------------
    // DATA
    // --------------------------------------------------------

    Serial.println();
    Serial.println(
        "================================="
    );

    Serial.println(
        "       STEP 8 DATA"
    );

    Serial.println(
        "================================="
    );


    if (
        !sim7600Service.connectInternet()
    )
    {
        Serial.println();
        Serial.println(
            "INTERNET : FAILED"
        );

        Serial.println();
        Serial.println(
            "Le modem est sur le reseau,"
        );

        Serial.println(
            "mais la DATA Internet n'est pas configuree."
        );

        Serial.println(
            "Verifier SIM7600_APN dans hardware_config.h"
        );

        return;
    }


    // --------------------------------------------------------
    // SUCCESS
    // --------------------------------------------------------

    Serial.println();
    Serial.println(
        "*********************************"
    );

    Serial.println(
        "       STEP 8 : SUCCESS"
    );

    Serial.println(
        "*********************************"
    );
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
    delay(1000);
}