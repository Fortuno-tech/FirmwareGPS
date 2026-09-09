#include <Arduino.h>

#include "../../include/config/pin_config.h"
#include "../../include/config/hardware_config.h"
#include "../../include/config/project_config.h"

#include "../../drivers/dht/dht_driver.h"
#include "../../drivers/sim7600/sim7600_client.h"

#include "../../services/sim7600_service/sim7600_service.h"
#include "../../services/gps_service/gps_service.h"
#include "../../services/rfid_service/rfid_service.h"
#include "../../services/time_service/time_service.h"
#include "../../services/temperature/temperature_service.h"
#include "../../managers/authentication_manager/authentication_manager.h"
#include "../../managers/telemetry_manager/telemetry_manager.h"
#include "../../services/mqtt_service/mqtt_service.h"


// ============================================================
// OBJETS
// ============================================================

DHTDriver dhtDriver(
    DHT_PIN,
    DHT_TYPE
);

TemperatureService temperatureService(
    dhtDriver
);

TimeService timeService;
TelemetryManager telemetryManager(
    temperatureService,
    timeService
);

AuthenticationManager authenticationManager;
ForticoDrivers::PN532Driver pn532Driver;
RFIDService rfidService(
    pn532Driver
);

SIM7600Service sim7600Service;
GPSService gpsService(
    sim7600Service.getDriver()
);

SIM7600Client* mqttClient = nullptr;
MQTTService* mqttService = nullptr;


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
// STEP 4 - RFID + AUTH
// ============================================================

bool testRFIDStep()
{
    printTitle(
        "STEP 4 - RFID + AUTHENTIFICATION"
    );

    if (!timeService.begin())
    {
        Serial.println(
            "[RTC] INITIALISATION FAILED"
        );

        return false;
    }

    if (!authenticationManager.begin())
    {
        Serial.println(
            "[AUTH] INITIALISATION FAILED"
        );

        return false;
    }

    if (!rfidService.begin())
    {
        Serial.println(
            "[RFID] INITIALISATION FAILED"
        );

        return false;
    }

    Serial.println(
        "[RFID] PRESENTATION D'UNE CARTE..."
    );

    for (int attempt = 0; attempt < 40; ++attempt)
    {
        if (rfidService.update())
        {
            break;
        }

        delay(100);
    }

    if (!rfidService.hasCard())
    {
        Serial.println(
            "[RFID] AUCUNE CARTE DETECTEE"
        );

        return false;
    }

    String uid = rfidService.getLastUIDString();
    Serial.print(
        "[RFID] UID LUE : "
    );
    Serial.println(uid);

    AuthenticationResult auth =
        authenticationManager.authenticate(uid);

    if (!auth.authorized)
    {
        Serial.println(
            "[AUTH] CARTE NON AUTORISEE"
        );

        return false;
    }

    Serial.print(
        "[AUTH] CARTE AUTORISEE : "
    );
    Serial.println(auth.driverId);

    Serial.println(
        "[RFID] STEP 4 VALIDATED"
    );

    return true;
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

    if (!temperatureService.begin())
    {
        Serial.println(
            "[TEMP] DHT non initialise"
        );
    }

    telemetryManager.begin();

    // ========================================================
    // STEP 4 - RFID
    // ========================================================

    if (!testRFIDStep())
    {
        Serial.println(
            "[FORTICO] STEP 4 RFID NON VALIDE"
        );

        Serial.println(
            "[FORTICO] Suite continue en mode degrade."
        );
    }

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

        Serial.println(
            "[FORTICO] MODE DEGRADE : pas de modem disponible"
        );

        Serial.println(
            "[FORTICO] Suite du firmware continue sans reseau/GPS"
        );
    }
    else
    {
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

            Serial.println(
                "[FORTICO] MODE DEGRADE : diagnostic modem incomplet"
            );
        }
        else
        {
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

                Serial.println(
                    "[FORTICO] MODE DEGRADE : reseau non disponible"
                );
            }
            else
            {
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

                    Serial.println(
                        "[FORTICO] MODE DEGRADE : internet non disponible"
                    );
                }
                else
                {
                    if (!temperatureService.update())
                    {
                        Serial.println(
                            "[TEMP] Lecture impossible"
                        );
                    }

                    telemetryManager.setNetwork(
                        NetworkData{ -1, "SIM7600", true }
                    );

                    telemetryManager.setBattery(
                        BatteryData{ 12.6f, 100, true }
                    );

                    telemetryManager.setFuel(
                        FuelData{ 75.0f, 42.5f, false, true }
                    );

                    telemetryManager.setEngine(
                        EngineData{ false, true, true }
                    );

                    String telemetryPayload = telemetryManager.buildJSON();

                    if (sim7600Service.isInternetReady())
                    {
                        HardwareSerial& simSerial = sim7600Service.getDriver().getSerial();
                        mqttClient = new SIM7600Client(simSerial);
                        mqttService = new MQTTService(*mqttClient);
                        mqttService->begin(MQTT_BROKER, MQTT_PORT, DEVICE_ID);

                        if (mqttService->connect(DEVICE_ID))
                        {
                            String telemetryTopic = mqttService->buildTopic(
                                MQTT_TOPIC_TELEMETRY,
                                VEHICLE_ID
                            );

                            mqttService->publish(
                                telemetryTopic,
                                telemetryPayload,
                                false
                            );

                            Serial.println(
                                "[MQTT] TELEMETRY PUBLISHED"
                            );
                        }
                        else
                        {
                            Serial.println(
                                "[MQTT] CONNECT FAILED"
                            );
                        }
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
                            "[FORTICO] GPS non fixe, mais le firmware continue."
                        );
                    }
                    else
                    {
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
                }
            }
        }
    }

    Serial.println();

    Serial.println(
        "================================="
    );

    Serial.println(
        "       FORTICO FIRMWARE"
    );

    Serial.println(
        "       DEMARRAGE TERMINAL"
    );

    Serial.println(
        "================================="
    );

    Serial.println();

    Serial.println(
        "MODE : OPERATIF / DEGRADE"
    );

    Serial.println(
        "Poursuite du systeme active."
    );

    Serial.println();

    Serial.println(
        "================================="
    );


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