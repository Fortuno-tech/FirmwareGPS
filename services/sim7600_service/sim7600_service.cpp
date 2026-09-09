#include "sim7600_service.h"

#include "../../include/config/hardware_config.h"


// =====================================================
// CONSTRUCTEUR
// =====================================================

SIM7600Service::SIM7600Service()
{
    _internetReady = false;

    _ipAddress = "";
}


// =====================================================
// BEGIN
// =====================================================

bool SIM7600Service::begin()
{
    Serial.println();
    Serial.println("=================================");
    Serial.println("       SIM7600 SERVICE");
    Serial.println("=================================");

    if (!_driver.begin())
    {
        Serial.println(
            "[SIM7600 SERVICE] INIT FAILED"
        );

        return false;
    }

    Serial.println(
        "[SIM7600 SERVICE] INIT OK"
    );

    return true;
}


// =====================================================
// DIAGNOSTICS
// =====================================================

bool SIM7600Service::runDiagnostics()
{
    Serial.println();
    Serial.println("=================================");
    Serial.println("       SIM7600 DIAGNOSTICS");
    Serial.println("=================================");

    bool modem = _driver.getModemInformation();

    bool sim = _driver.testSIM();

    bool signal = _driver.testSignal();

    bool network = _driver.testNetwork();

    bool operatorOK = _driver.testOperator();

    bool cpsi = _driver.testCPSI();

    bool attach = _driver.testPacketAttach();

    Serial.println();
    Serial.println(
        "---------- RESULTATS ----------"
    );

    Serial.print("MODEM    : ");
    Serial.println(
        modem ? "OK" : "FAILED"
    );

    Serial.print("SIM      : ");
    Serial.println(
        sim ? "OK" : "FAILED"
    );

    Serial.print("SIGNAL   : ");
    Serial.println(
        signal ? "OK" : "FAILED"
    );

    Serial.print("RESEAU   : ");
    Serial.println(
        network ? "OK" : "FAILED"
    );

    Serial.print("OPERATEUR: ");
    Serial.println(
        operatorOK ? "OK" : "FAILED"
    );

    Serial.print("CPSI     : ");
    Serial.println(
        cpsi ? "OK" : "FAILED"
    );

    Serial.print("CGATT    : ");
    Serial.println(
        attach ? "OK" : "FAILED"
    );

    return (
        modem &&
        sim &&
        signal &&
        network &&
        operatorOK &&
        cpsi &&
        attach
    );
}


// =====================================================
// WAIT NETWORK
// =====================================================

bool SIM7600Service::waitForNetwork()
{
    return _driver.waitForNetwork();
}


// =====================================================
// CONNECT INTERNET
// =====================================================

bool SIM7600Service::connectInternet()
{
    Serial.println();
    Serial.println("=================================");
    Serial.println("       CONNEXION INTERNET");
    Serial.println("=================================");

    // -------------------------------------------------
    // APN
    // -------------------------------------------------

    if (
        String(SIM7600_APN).length() == 0
    )
    {
        Serial.println(
            "[INTERNET] APN NON CONFIGURE"
        );

        return false;
    }

    if (
        !_driver.configureAPN(
            SIM7600_APN
        )
    )
    {
        Serial.println(
            "[INTERNET] APN FAILED"
        );

        return false;
    }

    // -------------------------------------------------
    // DATA
    // -------------------------------------------------

    if (
        !_driver.activateData()
    )
    {
        Serial.println(
            "[INTERNET] DATA ACTIVATION FAILED"
        );

        return false;
    }

    delay(3000);

    // -------------------------------------------------
    // IP
    // -------------------------------------------------

    if (
        !_driver.getIPAddress(
            _ipAddress
        )
    )
    {
        Serial.println(
            "[INTERNET] IP FAILED"
        );

        return false;
    }

    if (
        _ipAddress == "0.0.0.0" ||
        _ipAddress.length() == 0
    )
    {
        Serial.println(
            "[INTERNET] IP NON VALIDE"
        );

        return false;
    }

    _internetReady = true;

    Serial.println();
    Serial.println("=================================");
    Serial.println("       INTERNET READY");
    Serial.println("=================================");

    Serial.print("APN : ");

    Serial.println(
        SIM7600_APN
    );

    Serial.print("IP  : ");

    Serial.println(
        _ipAddress
    );

    Serial.println(
        "DATA: CONNECTED"
    );

    return true;
}


// =====================================================
// INTERNET READY
// =====================================================

bool SIM7600Service::isInternetReady()
{
    return _internetReady;
}


// =====================================================
// GET IP
// =====================================================

String SIM7600Service::getIPAddress()
{
    return _ipAddress;
}


// =====================================================
// GET DRIVER
// =====================================================

SIM7600Driver& SIM7600Service::getDriver()
{
    return _driver;
}