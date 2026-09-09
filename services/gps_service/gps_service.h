#ifndef GPS_SERVICE_H
#define GPS_SERVICE_H

#include <Arduino.h>
#include "../../drivers/sim7600/sim7600_driver.h"

// =====================================================
// STRUCTURE DONNEES GPS
// =====================================================

struct GPSData
{
    bool valid;

    // -------------------------------------------------
    // Coordonnees brutes du SIM7600
    // Format : DDMM.MMMMMM
    // Exemple : 2127.574621
    // -------------------------------------------------

    String latitudeRaw;
    String longitudeRaw;

    // -------------------------------------------------
    // Coordonnees converties
    // Format : degres decimaux
    // Exemple : -21.459577
    // -------------------------------------------------

    double latitude;
    double longitude;

    // -------------------------------------------------
    // Directions
    // -------------------------------------------------

    char latitudeDirection;
    char longitudeDirection;

    // -------------------------------------------------
    // Informations GPS
    // -------------------------------------------------

    String date;
    String time;
    String altitude;
    String speed;
    String course;
};


// =====================================================
// CLASSE GPS SERVICE
// =====================================================

class GPSService
{
public:

    GPSService(SIM7600Driver& driver);

    // Initialisation
    bool begin();

    // Activation GPS
    bool enableGPS();

    // Lecture GPS
    bool readGPS();

    // Attente d'un FIX satellite
    bool waitForFix();

    // Etat
    bool hasFix() const;

    // Recuperation des donnees
    GPSData getData() const;


private:

    // Driver SIM7600
    SIM7600Driver& _driver;

    // Donnees GPS
    GPSData _data;

    // Etats
    bool _gpsEnabled;
    bool _hasFix;

    // Analyse de +CGPSINFO
    bool parseGPSInfo(const String& response);

    // Conversion DDMM.MMMMMM -> degres decimaux
    double convertNMEAToDecimal(
        const String& coordinate,
        char direction
    );
};

#endif