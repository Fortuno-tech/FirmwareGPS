#include <Arduino.h>

#include "../../include/config/pin_config.h"
#include "../../include/config/hardware_config.h"

#include "../../services/sim7600_service/sim7600_service.h"
#include "../../services/gps_service/gps_service.h"


// ============================================================
// OBJETS
// ============================================================

SIM7600Service sim7600Service;

GPSService gpsService(
    sim7600Service.getDriver()
);


// ============================================================
// TITRE
// ============================================================

void printTitle(
    const char* title
)
{
    Serial.println();
    Serial.println("=================================");
    Serial.println(title);
    Serial.println("=================================");
}


// ============================================================
// STEP 10 - INTERNET
// ============================================================

bool testInternetStep()
{
    printTitle(
        "STEP 10 - CONNEXION INTERNET"
    );


    Serial.println(
        "[INTERNET] Configuration APN"
    );


    if (
        !sim7600Service.connectInternet()
    )
    {
        Serial.println();

        Serial.println(
            "[INTERNET] ECHEC CONNEXION"
        );

        return false;
    }


    String ipAddress =
        sim7600Service.getIPAddress();


    Serial.println();

    Serial.println(
        "================================="
    );

    Serial.println(
        "       INTERNET READY"
    );

    Serial.println(
        "================================="
    );


    Serial.print("APN : ");

    Serial.println(
        SIM7600_APN
    );


    Serial.print("IP  : ");

    Serial.println(
        ipAddress
    );


    Serial.println(
        "DATA: CONNECTED"
    );


    Serial.println(
        "================================="
    );

    Serial.println(
        "       STEP 10 VALIDATED"
    );

    Serial.println(
        "================================="
    );


    Serial.println(
        "SIM7600 : OK"
    );

    Serial.println(
        "RESEAU  : OK"
    );


    Serial.print("APN     : ");

    Serial.println(
        SIM7600_APN
    );


    Serial.println(
        "DATA    : OK"
    );


    Serial.print("IP      : ");

    Serial.println(
        ipAddress
    );


    Serial.println(
        "INTERNET: READY"
    );


    Serial.println(
        "================================="
    );


    return true;
}


// ============================================================
// STEP 11 - GPS
// ============================================================

bool testGPSStep()
{
    printTitle(
        "STEP 11 - GPS"
    );


    // --------------------------------------------------------
    // INITIALISATION
    // --------------------------------------------------------

    Serial.println(
        "[GPS SERVICE] Initialisation..."
    );


    if (
        !gpsService.begin()
    )
    {
        Serial.println(
            "[GPS SERVICE] INIT FAILED"
        );

        return false;
    }


    Serial.println(
        "[GPS SERVICE] INIT OK"
    );


    // --------------------------------------------------------
    // ATTENTE FIX
    // --------------------------------------------------------

    if (
        gpsService.waitForFix()
    )
    {
        Serial.println();

        Serial.println(
            "================================="
        );

        Serial.println(
            "       STEP 11 VALIDATED"
        );

        Serial.println(
            "================================="
        );

        return true;
    }


    // --------------------------------------------------------
    // PAS DE FIX
    // --------------------------------------------------------

    Serial.println();

    Serial.println(
        "================================="
    );

    Serial.println(
        "       GPS SANS FIX"
    );

    Serial.println(
        "================================="
    );

    Serial.println();

    Serial.println(
        "[GPS] Le modem fonctionne."
    );

    Serial.println(
        "[GPS] Le GPS est active."
    );

    Serial.println(
        "[GPS] Aucune position satellite."
    );

    Serial.println();

    Serial.println(
        "[GPS] Verifier :"
    );

    Serial.println(
        "1. Antenne GNSS branchee"
    );

    Serial.println(
        "2. Bon connecteur GNSS"
    );

    Serial.println(
        "3. Test a l'exterieur"
    );

    Serial.println(
        "4. Ciel degage"
    );

    Serial.println(
        "5. Attendre l'acquisition"
    );

    Serial.println();


    /*
     * IMPORTANT :
     *
     * Ce n'est pas une panne du SIM7600.
     *
     * STEP 11 est simplement NON VALIDE
     * tant qu'une position GPS n'est pas obtenue.
     */

    return false;
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
    // --------------------------------------------------------
    // SERIAL DEBUG
    // --------------------------------------------------------

    Serial.begin(
        115200
    );


    delay(1000);


    // --------------------------------------------------------
    // BANNER
    // --------------------------------------------------------

    Serial.println();

    Serial.println(
        "================================="
    );

    Serial.println(
        "       FORTICO FIRMWARE"
    );

    Serial.println(
        "       SIM7600 GPS TEST"
    );

    Serial.println(
        "================================="
    );

    Serial.println();


    Serial.println(
        "[SYSTEM] Demarrage ESP32..."
    );


    // ========================================================
    // SIM7600
    // ========================================================

    printTitle(
        "SIM7600 INITIALISATION"
    );


    if (
        !sim7600Service.begin()
    )
    {
        Serial.println();

        Serial.println(
            "[FORTICO] SIM7600 INITIALISATION FAILED"
        );

        return;
    }


    Serial.println();

    Serial.println(
        "[FORTICO] SIM7600 INITIALISATION OK"
    );


    // ========================================================
    // DIAGNOSTICS
    // ========================================================

    printTitle(
        "DIAGNOSTIC SIM7600"
    );


    if (
        !sim7600Service.runDiagnostics()
    )
    {
        Serial.println();

        Serial.println(
            "[FORTICO] DIAGNOSTIC FAILED"
        );

        return;
    }


    Serial.println();

    Serial.println(
        "[FORTICO] DIAGNOSTIC OK"
    );


    // ========================================================
    // RESEAU
    // ========================================================

    printTitle(
        "ATTENTE RESEAU"
    );


    if (
        !sim7600Service.waitForNetwork()
    )
    {
        Serial.println();

        Serial.println(
            "[FORTICO] RESEAU NON DISPONIBLE"
        );

        return;
    }


    Serial.println(
        "[FORTICO] RESEAU OK"
    );


    // ========================================================
    // STEP 10
    // ========================================================

    if (
        !testInternetStep()
    )
    {
        Serial.println();

        Serial.println(
            "[FORTICO] STEP 10 FAILED"
        );

        return;
    }


    // ========================================================
    // STEP 11
    // ========================================================

    if (
        !testGPSStep()
    )
    {
        Serial.println();

        Serial.println(
            "[FORTICO] STEP 11 NON VALIDE"
        );

        Serial.println();

        Serial.println(
            "[FORTICO] Arret apres test GPS."
        );

        return;
    }


    // ========================================================
    // VALIDATION
    // ========================================================

    Serial.println();

    Serial.println(
        "================================="
    );

    Serial.println(
        "       FORTICO FIRMWARE"
    );

    Serial.println(
        "       VALIDATION TERMINEE"
    );

    Serial.println(
        "================================="
    );

    Serial.println();

    Serial.println(
        "STEP 10 : INTERNET -> OK"
    );

    Serial.println(
        "STEP 11 : GPS      -> OK"
    );

    Serial.println();

    Serial.println(
        "MQTT : PRET POUR STEP 12"
    );

    Serial.println();

    Serial.println(
        "================================="
    );
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
    /*
     * Pas de traitement continu pour le moment.
     *
     * Le projet est encore en phase de validation
     * SIM7600 / Internet / GPS.
     */

    delay(1000);
}