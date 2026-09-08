#ifndef SIM7600_DRIVER_H
#define SIM7600_DRIVER_H

#include <Arduino.h>

class SIM7600Driver
{
public:

    SIM7600Driver();

    // Initialisation
    bool begin();

    // Tests modem
    bool testAT();

    // Informations modem
    bool getModemInformation(String& information);

    // Tests SIM
    bool testSIM();

    // Test signal
    bool testSignal();

    // Test réseau
    bool testNetwork();

    // Test opérateur
    bool testOperator();

    // Test CPSI
    bool testCPSI();

    // Test attach data
    bool testPacketAttach();

    // Attente réseau
    bool waitForNetwork(uint32_t timeoutMs);

    // Configuration APN
    bool configureAPN(const String& apn);

    // Activation data
    bool activateData();

    // Récupération IP
    bool getIPAddress(String& ipAddress);

    // Test Internet
    bool testInternet();

    // Etat data
    bool isDataConnected() const;

    // Commande AT générique
    String sendCommand(
        const String& command,
        uint32_t timeout = 2000
    );

    // Etat modem
    bool isReady() const;

private:

    HardwareSerial* _serial;

    bool _ready;
    bool _dataConnected;

    bool isNetworkRegistered(
        const String& response
    );

    bool isSignalAvailable(
        const String& response
    );

    bool isServiceAvailable(
        const String& response
    );
};

#endif