#ifndef GPS_SERVICE_H
#define GPS_SERVICE_H

#include <Arduino.h>

#include "../../drivers/sim7600/sim7600_driver.h"


// ============================================================
// STRUCTURE GPS
// ============================================================

struct GPSData
{
    bool valid;

    String latitude;
    String longitude;

    char latitudeDirection;
    char longitudeDirection;

    String date;
    String time;

    String altitude;
    String speed;
    String course;
};


// ============================================================
// GPS SERVICE
// ============================================================

class GPSService
{
public:

    GPSService(
        SIM7600Driver& driver
    );


    // --------------------------------------------------------
    // Initialisation
    // --------------------------------------------------------

    bool begin();


    // --------------------------------------------------------
    // GPS
    // --------------------------------------------------------

    bool enableGPS();

    bool readGPS();

    bool waitForFix();


    // --------------------------------------------------------
    // Etat
    // --------------------------------------------------------

    bool hasFix() const;


    // --------------------------------------------------------
    // Donnees
    // --------------------------------------------------------

    GPSData getData() const;


private:

    SIM7600Driver& _driver;

    GPSData _data;

    bool _gpsEnabled;

    bool _hasFix;


    // --------------------------------------------------------
    // Parser
    // --------------------------------------------------------

    bool parseGPSInfo(
        const String& response
    );
};

#endif