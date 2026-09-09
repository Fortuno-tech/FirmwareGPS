#include "gps_service.h"

#include "../../include/config/hardware_config.h"


// =====================================================
// CONSTRUCTEUR
// =====================================================

GPSService::GPSService(SIM7600Driver& driver)
    : _driver(driver)
{
    // -------------------------------------------------
    // Etat GPS
    // -------------------------------------------------

    _gpsEnabled = false;
    _hasFix = false;

    // -------------------------------------------------
    // Etat donnees
    // -------------------------------------------------

    _data.valid = false;

    // -------------------------------------------------
    // Coordonnees RAW
    // -------------------------------------------------

    _data.latitudeRaw = "";
    _data.longitudeRaw = "";

    // -------------------------------------------------
    // Coordonnees decimal
    // -------------------------------------------------

    _data.latitude = 0.0;
    _data.longitude = 0.0;

    // -------------------------------------------------
    // Directions
    // -------------------------------------------------

    _data.latitudeDirection = '\0';
    _data.longitudeDirection = '\0';

    // -------------------------------------------------
    // Informations GPS
    // -------------------------------------------------

    _data.date = "";
    _data.time = "";
    _data.altitude = "";
    _data.speed = "";
    _data.course = "";
}


// =====================================================
// BEGIN
// =====================================================

bool GPSService::begin()
{
    Serial.println();
    Serial.println("=================================");
    Serial.println("          GPS SERVICE");
    Serial.println("=================================");
    Serial.println();

    // -------------------------------------------------
    // Reset etat
    // -------------------------------------------------

    _gpsEnabled = false;
    _hasFix = false;

    _data.valid = false;

    // -------------------------------------------------
    // Activation GPS
    // -------------------------------------------------

    if (!enableGPS())
    {
        Serial.println("[GPS SERVICE] GPS INIT FAILED");
        return false;
    }

    // -------------------------------------------------
    // Verification GPS
    // -------------------------------------------------

    if (!_driver.gpsGetStatus())
    {
        Serial.println("[GPS SERVICE] GPS STATUS FAILED");
        return false;
    }

    Serial.println("[GPS SERVICE] INIT OK");

    return true;
}


// =====================================================
// ENABLE GPS
// =====================================================

bool GPSService::enableGPS()
{
    // -------------------------------------------------
    // GPS deja actif ?
    // -------------------------------------------------

    if (_gpsEnabled)
    {
        Serial.println("[GPS] GPS deja actif dans le service");
        return true;
    }

    // -------------------------------------------------
    // Demande activation au driver SIM7600
    // -------------------------------------------------

    if (!_driver.gpsEnable())
    {
        Serial.println("[GPS] ECHEC ACTIVATION");
        return false;
    }

    // -------------------------------------------------
    // Etat interne
    // -------------------------------------------------

    _gpsEnabled = true;

    Serial.println("[GPS] GPS ACTIVE");

    return true;
}


// =====================================================
// READ GPS
// =====================================================

bool GPSService::readGPS()
{
    // -------------------------------------------------
    // Verification activation
    // -------------------------------------------------

    if (!_gpsEnabled)
    {
        Serial.println("[GPS] GPS non active");
        return false;
    }

    Serial.println("[GPS] Lecture position...");

    // -------------------------------------------------
    // Lecture AT+CGPSINFO
    // -------------------------------------------------

    String response = _driver.sendCommand(
        "AT+CGPSINFO",
        GPS_INFO_TIMEOUT_MS
    );

    // -------------------------------------------------
    // Verification reponse
    // -------------------------------------------------

    if (response.length() == 0)
    {
        Serial.println("[GPS] Reponse vide");
        return false;
    }

    Serial.println("[GPS] Reponse recue :");
    Serial.println(response);

    // -------------------------------------------------
    // Analyse
    // -------------------------------------------------

    return parseGPSInfo(response);
}


// =====================================================
// WAIT FOR FIX
// =====================================================

bool GPSService::waitForFix()
{
    // -------------------------------------------------
    // Verification GPS
    // -------------------------------------------------

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

    Serial.print("[GPS] Attente maximale : ");
    Serial.print(GPS_START_TIMEOUT_MS / 1000);
    Serial.println(" secondes");

    Serial.println();

    Serial.println("[GPS] IMPORTANT :");
    Serial.println("[GPS] - Test a l'exterieur");
    Serial.println("[GPS] - Ciel degage");
    Serial.println("[GPS] - Antenne GNSS branchee");
    Serial.println("[GPS] - Ne pas deplacer l'antenne");

    Serial.println();

    // -------------------------------------------------
    // Timer
    // -------------------------------------------------

    uint32_t startTime = millis();

    uint32_t lastRead = 0;

    // -------------------------------------------------
    // Boucle attente FIX
    // -------------------------------------------------

    while (
        millis() - startTime <
        GPS_START_TIMEOUT_MS
    )
    {
        // -------------------------------------------------
        // Lecture periodique
        // -------------------------------------------------

        if (
            millis() - lastRead >=
            GPS_FIX_CHECK_INTERVAL_MS
        )
        {
            lastRead = millis();

            Serial.println();
            Serial.println("[GPS] Lecture position...");

            // -------------------------------------------------
            // Lecture GPS
            // -------------------------------------------------

            if (readGPS())
            {
                // -------------------------------------------------
                // FIX obtenu
                // -------------------------------------------------

                if (_hasFix)
                {
                    Serial.println();
                    Serial.println("=================================");
                    Serial.println("       GPS FIX OK");
                    Serial.println("=================================");

                    return true;
                }
            }

            // -------------------------------------------------
            // Temps ecoule
            // -------------------------------------------------

            uint32_t elapsed =
                (millis() - startTime) / 1000;

            Serial.print("[GPS] Temps ecoule : ");
            Serial.print(elapsed);
            Serial.println(" s");
        }

        delay(100);
    }

    // -------------------------------------------------
    // Timeout
    // -------------------------------------------------

    Serial.println();
    Serial.println("=================================");
    Serial.println("       GPS FIX TIMEOUT");
    Serial.println("=================================");
    Serial.println();

    Serial.println("[GPS] Aucun FIX satellite.");

    return false;
}


// =====================================================
// CONVERSION NMEA
// DDMM.MMMMMM -> DEGRES DECIMAUX
// =====================================================

double GPSService::convertNMEAToDecimal(
    const String& coordinate,
    char direction
)
{
    // -------------------------------------------------
    // Verification
    // -------------------------------------------------

    if (coordinate.length() == 0)
    {
        return 0.0;
    }

    // -------------------------------------------------
    // Conversion String -> double
    //
    // Exemple :
    //
    // "2127.574621"
    //
    // devient :
    //
    // 2127.574621
    // -------------------------------------------------

    double value = coordinate.toDouble();

    // -------------------------------------------------
    // Extraction des degres
    //
    // 2127.574621 / 100
    //
    // = 21
    //
    // Donc :
    //
    // degrees = 21
    // -------------------------------------------------

    int degrees = (int)(value / 100.0);

    // -------------------------------------------------
    // Extraction minutes
    //
    // 2127.574621
    // - 2100
    //
    // = 27.574621
    // -------------------------------------------------

    double minutes =
        value - (degrees * 100.0);

    // -------------------------------------------------
    // Conversion minutes -> degres
    //
    // 27.574621 / 60
    // = 0.459577...
    // -------------------------------------------------

    double decimal =
        degrees + (minutes / 60.0);

    // -------------------------------------------------
    // Sud = negatif
    // Ouest = negatif
    // -------------------------------------------------

    if (
        direction == 'S' ||
        direction == 'W'
    )
    {
        decimal = -decimal;
    }

    return decimal;
}


// =====================================================
// PARSE CGPSINFO
// =====================================================

bool GPSService::parseGPSInfo(
    const String& response
)
{
    // -------------------------------------------------
    // Recherche +CGPSINFO:
    // -------------------------------------------------

    int index =
        response.indexOf("+CGPSINFO:");

    if (index < 0)
    {
        Serial.println("[GPS] +CGPSINFO ABSENT");

        _hasFix = false;
        _data.valid = false;

        return false;
    }

    // -------------------------------------------------
    // Extraction ligne
    // -------------------------------------------------

    String line =
        response.substring(index);

    // -------------------------------------------------
    // Supprimer les lignes suivantes
    // -------------------------------------------------

    int endLine =
        line.indexOf('\n');

    if (endLine >= 0)
    {
        line =
            line.substring(0, endLine);
    }

    line.trim();

    Serial.print("[GPS] DATA : ");
    Serial.println(line);

    // -------------------------------------------------
    // Recherche :
    //
    // +CGPSINFO:
    // -------------------------------------------------

    int colon =
        line.indexOf(':');

    if (colon < 0)
    {
        Serial.println("[GPS] Format invalide");

        return false;
    }

    // -------------------------------------------------
    // Recuperation donnees apres :
    //
    // +CGPSINFO: 2127.574621,S,...
    // -------------------------------------------------

    String data =
        line.substring(colon + 1);

    data.trim();

    // -------------------------------------------------
    // Pas de FIX
    //
    // +CGPSINFO: ,,,,,,,,
    // -------------------------------------------------

    if (
        data.length() == 0 ||
        data == ",,,,,,,,"
    )
    {
        Serial.println("[GPS] PAS DE FIX SATELLITE");

        _hasFix = false;
        _data.valid = false;

        return false;
    }

    // -------------------------------------------------
    // Tableau des champs
    //
    // 0 = latitude
    // 1 = N/S
    // 2 = longitude
    // 3 = E/W
    // 4 = date
    // 5 = heure
    // 6 = altitude
    // 7 = vitesse
    // 8 = course
    // -------------------------------------------------

    String fields[9];

    int fieldIndex = 0;

    int start = 0;

    // -------------------------------------------------
    // Separation CSV
    // -------------------------------------------------

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
                    data.substring(start, i);

                fields[fieldIndex].trim();
            }

            fieldIndex++;

            start = i + 1;
        }
    }

    // -------------------------------------------------
    // Verification latitude / longitude
    // -------------------------------------------------

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

    // =================================================
    // DONNEES RAW
    // =================================================

    _data.latitudeRaw =
        fields[0];

    _data.latitudeDirection =
        fields[1].charAt(0);

    _data.longitudeRaw =
        fields[2];

    _data.longitudeDirection =
        fields[3].charAt(0);

    // =================================================
    // CONVERSION DEGRES DECIMAUX
    // =================================================

    _data.latitude =
        convertNMEAToDecimal(
            _data.latitudeRaw,
            _data.latitudeDirection
        );

    _data.longitude =
        convertNMEAToDecimal(
            _data.longitudeRaw,
            _data.longitudeDirection
        );

    // =================================================
    // AUTRES DONNEES
    // =================================================

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

    // =================================================
    // VALIDATION
    // =================================================

    _data.valid = true;

    _hasFix = true;

    // =================================================
    // AFFICHAGE
    // =================================================

    Serial.println();
    Serial.println("=================================");
    Serial.println("       POSITION GPS");
    Serial.println("=================================");

    // -------------------------------------------------
    // RAW
    // -------------------------------------------------

    Serial.print("Latitude RAW  : ");
    Serial.print(_data.latitudeRaw);
    Serial.print(" ");
    Serial.println(_data.latitudeDirection);

    Serial.print("Longitude RAW : ");
    Serial.print(_data.longitudeRaw);
    Serial.print(" ");
    Serial.println(_data.longitudeDirection);

    // -------------------------------------------------
    // DECIMAL
    // -------------------------------------------------

    Serial.print("Latitude      : ");
    Serial.print(_data.latitude, 6);
    Serial.println(" deg");

    Serial.print("Longitude     : ");
    Serial.print(_data.longitude, 6);
    Serial.println(" deg");

    // -------------------------------------------------
    // Informations
    // -------------------------------------------------

    Serial.print("Date          : ");
    Serial.println(_data.date);

    Serial.print("Heure         : ");
    Serial.println(_data.time);

    Serial.print("Altitude      : ");
    Serial.print(_data.altitude);
    Serial.println(" m");

    Serial.print("Vitesse       : ");
    Serial.println(_data.speed);

    Serial.print("Course        : ");
    Serial.println(_data.course);

    Serial.println("=================================");

    return true;
}


// =====================================================
// HAS FIX
// =====================================================

bool GPSService::hasFix() const
{
    return _hasFix;
}


// =====================================================
// GET DATA
// =====================================================

GPSData GPSService::getData() const
{
    return _data;
}