#include "gps_service.h"

#include "../../include/config/hardware_config.h"


// ============================================================
// CONSTRUCTEUR
// ============================================================

GPSService::GPSService(
    SIM7600Driver& driver
)
    : _driver(driver)
{
    _gpsEnabled = false;

    _hasFix = false;


    _data.valid = false;

    _data.latitude = "";
    _data.longitude = "";

    _data.latitudeDirection = '\0';
    _data.longitudeDirection = '\0';

    _data.date = "";
    _data.time = "";

    _data.altitude = "";
    _data.speed = "";
    _data.course = "";
}


// ============================================================
// BEGIN
// ============================================================

bool GPSService::begin()
{
    Serial.println();
    Serial.println("=================================");
    Serial.println("          GPS SERVICE");
    Serial.println("=================================");
    Serial.println();


    _gpsEnabled = false;

    _hasFix = false;

    _data.valid = false;


    // --------------------------------------------------------
    // Activation GPS
    // --------------------------------------------------------

    if (!enableGPS())
    {
        Serial.println("[GPS SERVICE] GPS INIT FAILED");
        return false;
    }


    // --------------------------------------------------------
    // Vérification
    // --------------------------------------------------------

    if (!_driver.gpsGetStatus())
    {
        Serial.println("[GPS SERVICE] GPS STATUS FAILED");
        return false;
    }


    Serial.println("[GPS SERVICE] INIT OK");

    return true;
}


// ============================================================
// ENABLE GPS
// ============================================================

bool GPSService::enableGPS()
{
    if (_gpsEnabled)
    {
        Serial.println("[GPS] GPS deja actif dans le service");
        return true;
    }


    if (!_driver.gpsEnable())
    {
        Serial.println("[GPS] ECHEC ACTIVATION");
        return false;
    }


    _gpsEnabled = true;

    Serial.println("[GPS] GPS ACTIVE");

    return true;
}


// ============================================================
// READ GPS
// ============================================================

bool GPSService::readGPS()
{
    if (!_gpsEnabled)
    {
        Serial.println("[GPS] GPS non active");

        return false;
    }


    Serial.println("[GPS] Lecture position...");


    String response = _driver.sendCommand(
        "AT+CGPSINFO",
        GPS_INFO_TIMEOUT_MS
    );


    if (response.length() == 0)
    {
        Serial.println("[GPS] Reponse vide");

        return false;
    }


    Serial.println("[GPS] Reponse recue :");

    Serial.println(response);


    return parseGPSInfo(response);
}


// ============================================================
// WAIT FOR FIX
// ============================================================

bool GPSService::waitForFix()
{
    if (!_gpsEnabled)
    {
        Serial.println("[GPS] GPS non active");
        return false;
    }


    Serial.println();
    Serial.println("=================================");
    Serial.println("       ATTENTE GPS FIX");
    Serial.println("=================================");

    Serial.println();

    Serial.println("[GPS] Recherche satellites...");

    Serial.println("[GPS] Attente maximale : "
                   + String(GPS_START_TIMEOUT_MS / 1000)
                   + " secondes");

    Serial.println();

    Serial.println("[GPS] IMPORTANT :");
    Serial.println("[GPS] - Test a l'exterieur");
    Serial.println("[GPS] - Ciel degage");
    Serial.println("[GPS] - Antenne GNSS branchee");
    Serial.println("[GPS] - Ne pas deplacer l'antenne");

    Serial.println();


    uint32_t startTime = millis();

    uint32_t lastRead = 0;


    while (
        millis() - startTime
        <
        GPS_START_TIMEOUT_MS
    )
    {
        // ----------------------------------------------------
        // Lecture toutes les 5 secondes
        // ----------------------------------------------------

        if (
            millis() - lastRead
            >=
            GPS_FIX_CHECK_INTERVAL_MS
        )
        {
            lastRead = millis();


            Serial.println();
            Serial.println("[GPS] Lecture position...");


            if (readGPS())
            {
                if (_hasFix)
                {
                    Serial.println();
                    Serial.println("=================================");
                    Serial.println("       GPS FIX OK");
                    Serial.println("=================================");

                    return true;
                }
            }


            // ------------------------------------------------
            // Temps écoulé
            // ------------------------------------------------

            uint32_t elapsed =
                (millis() - startTime) / 1000;


            Serial.print("[GPS] Temps ecoule : ");

            Serial.print(elapsed);

            Serial.println(" s");
        }


        delay(100);
    }


    // --------------------------------------------------------
    // TIMEOUT
    // --------------------------------------------------------

    Serial.println();

    Serial.println("=================================");
    Serial.println("       GPS FIX TIMEOUT");
    Serial.println("=================================");

    Serial.println();

    Serial.println("[GPS] Aucun FIX satellite.");

    return false;
}


// ============================================================
// PARSE GPS INFO
// ============================================================

bool GPSService::parseGPSInfo(
    const String& response
)
{
    int index = response.indexOf(
        "+CGPSINFO:"
    );


    if (index < 0)
    {
        Serial.println(
            "[GPS] +CGPSINFO ABSENT"
        );

        return false;
    }


    // --------------------------------------------------------
    // Extraction de la ligne
    // --------------------------------------------------------

    String line =
        response.substring(
            index
        );


    int endLine =
        line.indexOf('\n');


    if (endLine >= 0)
    {
        line =
            line.substring(
                0,
                endLine
            );
    }


    line.trim();


    Serial.print("[GPS] DATA : ");

    Serial.println(line);


    // --------------------------------------------------------
    // Retirer "+CGPSINFO:"
    // --------------------------------------------------------

    int colon =
        line.indexOf(':');


    if (colon < 0)
    {
        return false;
    }


    String data =
        line.substring(
            colon + 1
        );


    data.trim();


    // --------------------------------------------------------
    // Aucun FIX
    // --------------------------------------------------------

    if (
        data.length() == 0 ||
        data == ",,,,,,,,"
    )
    {
        Serial.println(
            "[GPS] PAS DE FIX SATELLITE"
        );

        _hasFix = false;

        _data.valid = false;

        return false;
    }


    // --------------------------------------------------------
    // CSV
    // --------------------------------------------------------

    String fields[9];

    int fieldIndex = 0;

    int start = 0;


    for (
        int i = 0;
        i <= data.length();
        i++
    )
    {
        if (
            i == data.length() ||
            data.charAt(i) == ','
        )
        {
            if (fieldIndex < 9)
            {
                fields[fieldIndex] =
                    data.substring(
                        start,
                        i
                    );

                fields[fieldIndex].trim();
            }

            fieldIndex++;

            start = i + 1;
        }
    }


    // --------------------------------------------------------
    // Vérification latitude / longitude
    // --------------------------------------------------------

    if (
        fields[0].length() == 0 ||
        fields[1].length() == 0 ||
        fields[2].length() == 0 ||
        fields[3].length() == 0
    )
    {
        Serial.println(
            "[GPS] LATITUDE/LONGITUDE ABSENTE"
        );

        _hasFix = false;

        _data.valid = false;

        return false;
    }


    // --------------------------------------------------------
    // Stockage
    // --------------------------------------------------------

    _data.latitude =
        fields[0];

    _data.latitudeDirection =
        fields[1].charAt(0);

    _data.longitude =
        fields[2];

    _data.longitudeDirection =
        fields[3].charAt(0);


    if (fieldIndex > 4)
    {
        _data.date =
            fields[4];
    }


    if (fieldIndex > 5)
    {
        _data.time =
            fields[5];
    }


    if (fieldIndex > 6)
    {
        _data.altitude =
            fields[6];
    }


    if (fieldIndex > 7)
    {
        _data.speed =
            fields[7];
    }


    if (fieldIndex > 8)
    {
        _data.course =
            fields[8];
    }


    // --------------------------------------------------------
    // FIX VALIDE
    // --------------------------------------------------------

    _data.valid = true;

    _hasFix = true;


    Serial.println();
    Serial.println("=================================");
    Serial.println("       POSITION GPS");
    Serial.println("=================================");

    Serial.print("Latitude  : ");
    Serial.print(_data.latitude);
    Serial.print(" ");
    Serial.println(_data.latitudeDirection);

    Serial.print("Longitude : ");
    Serial.print(_data.longitude);
    Serial.print(" ");
    Serial.println(_data.longitudeDirection);

    Serial.print("Date      : ");
    Serial.println(_data.date);

    Serial.print("Heure     : ");
    Serial.println(_data.time);

    Serial.print("Altitude  : ");
    Serial.println(_data.altitude);

    Serial.print("Vitesse   : ");
    Serial.println(_data.speed);

    Serial.print("Course    : ");
    Serial.println(_data.course);

    Serial.println("=================================");


    return true;
}


// ============================================================
// HAS FIX
// ============================================================

bool GPSService::hasFix() const
{
    return _hasFix;
}


// ============================================================
// GET DATA
// ============================================================

GPSData GPSService::getData() const
{
    return _data;
}