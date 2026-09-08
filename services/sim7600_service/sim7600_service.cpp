#include "sim7600_service.h"

#include "hardware_config.h"


// =====================================================
// CONSTRUCTEUR
// =====================================================

SIM7600Service::SIM7600Service(
    SIM7600Driver& driver
)
    : _driver(driver)
{
    _ready = false;
}


// =====================================================
// BEGIN
// =====================================================

bool SIM7600Service::begin()
{
    Serial.println();
    Serial.println(
        "================================="
    );

    Serial.println(
        "     SIM7600 SERVICE"
    );

    Serial.println(
        "================================="
    );


    if (!_driver.begin())
    {
        Serial.println(
            "[SERVICE] Driver FAILED"
        );

        return false;
    }


    _ready = true;


    Serial.println(
        "[SERVICE] Driver SIM7600 READY"
    );


    return true;
}


// =====================================================
// DIAGNOSTIC
// =====================================================

bool SIM7600Service::runDiagnostics()
{
    if (!_ready)
    {
        return false;
    }


    String modemInfo;

    bool modemOK =
        _driver.getModemInformation(
            modemInfo
        );


    bool simOK =
        _driver.testSIM();


    bool signalOK =
        _driver.testSignal();


    bool networkOK =
        _driver.testNetwork();


    bool operatorOK =
        _driver.testOperator();


    bool cpsiOK =
        _driver.testCPSI();


    bool dataOK =
        _driver.testPacketAttach();


    Serial.println();
    Serial.println(
        "================================="
    );

    Serial.println(
        "       RESULTAT DIAGNOSTIC"
    );

    Serial.println(
        "================================="
    );


    Serial.print("MODEM     : ");
    Serial.println(
        modemOK ? "OK" : "FAILED"
    );


    Serial.print("SIM       : ");
    Serial.println(
        simOK ? "OK" : "FAILED"
    );


    Serial.print("SIGNAL    : ");
    Serial.println(
        signalOK ? "OK" : "FAILED"
    );


    Serial.print("RESEAU    : ");
    Serial.println(
        networkOK ? "OK" : "FAILED"
    );


    Serial.print("OPERATEUR : ");
    Serial.println(
        operatorOK ? "OK" : "FAILED"
    );


    Serial.print("CPSI      : ");
    Serial.println(
        cpsiOK ? "OK" : "FAILED"
    );


    Serial.print("DATA      : ");
    Serial.println(
        dataOK ? "ATTACHE" : "NON ATTACHE"
    );


    Serial.println(
        "================================="
    );


    return (
        modemOK &&
        simOK &&
        signalOK &&
        networkOK &&
        cpsiOK &&
        dataOK
    );
}


// =====================================================
// WAIT NETWORK
// =====================================================

bool SIM7600Service::waitForNetwork()
{
    return _driver.waitForNetwork(
        SIM7600_NETWORK_SEARCH_TIMEOUT_MS
    );
}


// =====================================================
// CONNECT INTERNET
// =====================================================

bool SIM7600Service::connectInternet()
{
    Serial.println();
    Serial.println(
        "================================="
    );

    Serial.println(
        "     CONNEXION INTERNET"
    );

    Serial.println(
        "================================="
    );


    // -------------------------------------------------
    // APN
    // -------------------------------------------------

    String apn =
        SIM7600_APN;


    if (apn.length() == 0)
    {
        Serial.println();
        Serial.println(
            "[INTERNET] APN NON CONFIGURE"
        );

        Serial.println(
            "[INTERNET] Modifier SIM7600_APN"
        );

        return false;
    }


    // -------------------------------------------------
    // Configuration APN
    // -------------------------------------------------

    if (
        !_driver.configureAPN(
            apn
        )
    )
    {
        return false;
    }


    // -------------------------------------------------
    // Activation DATA
    // -------------------------------------------------

    if (
        !_driver.activateData()
    )
    {
        return false;
    }


    // -------------------------------------------------
    // IP
    // -------------------------------------------------

    String ipAddress;


    if (
        !_driver.getIPAddress(
            ipAddress
        )
    )
    {
        return false;
    }


    Serial.println();
    Serial.println(
        "*********************************"
    );

    Serial.println(
        "   INTERNET CONNECTE"
    );

    Serial.print(
        "   IP : "
    );

    Serial.println(
        ipAddress
    );

    Serial.println(
        "*********************************"
    );


    return true;
}


// =====================================================
// GET IP
// =====================================================

bool SIM7600Service::getIPAddress(
    String& ipAddress
)
{
    return _driver.getIPAddress(
        ipAddress
    );
}


// =====================================================
// READY
// =====================================================

bool SIM7600Service::isReady() const
{
    return _ready;
}