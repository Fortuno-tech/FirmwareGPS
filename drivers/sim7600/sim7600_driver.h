#ifndef SIM7600_DRIVER_H
#define SIM7600_DRIVER_H

#include <Arduino.h>
#include <HardwareSerial.h>

class SIM7600Driver
{
public:

    SIM7600Driver();

    // ========================================================
    // MODEM
    // ========================================================

    bool begin();

    String sendCommand(
        const String& command,
        uint32_t timeout = 3000
    );

    bool testAT();
    bool getModemInformation();
    bool testSIM();
    bool testSignal();
    bool testNetwork();
    bool testOperator();
    bool testCPSI();
    bool testPacketAttach();

    bool waitForNetwork();


    // ========================================================
    // INTERNET
    // ========================================================

    bool configureAPN(const String& apn);

    bool activateData();

    bool getIPAddress(
        String& ipAddress
    );

    bool testInternet();

    bool isInternetConnected();


    // ========================================================
    // GPS
    // ========================================================

    bool gpsEnable();

    bool gpsDisable();

    bool gpsGetStatus();

    bool gpsGetRawInfo();


    // ========================================================
    // ETAT
    // ========================================================

    bool isReady() const;

    bool isDataConnected() const;

    HardwareSerial& getSerial();

private:

    HardwareSerial* _serial;

    bool _ready;

    bool _dataConnected;
};

#endif