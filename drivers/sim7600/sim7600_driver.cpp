#include "sim7600_driver.h"

#include "../../include/config/hardware_config.h"
#include "../../include/config/pin_config.h"


// ============================================================
// CONSTRUCTEUR
// ============================================================

SIM7600Driver::SIM7600Driver()
{
    _serial = nullptr;

    _ready = false;

    _dataConnected = false;
}


// ============================================================
// BEGIN
// ============================================================

bool SIM7600Driver::begin()
{
    Serial.println();
    Serial.println("=================================");
    Serial.println("       SIM7600 DRIVER");
    Serial.println("=================================");

    if (_serial == nullptr)
    {
        _serial = new HardwareSerial(
            SIM7600_UART_NUMBER
        );
    }

    _serial->begin(
        SIM7600_BAUDRATE,
        SERIAL_8N1,
        SIM7600_RX_PIN,
        SIM7600_TX_PIN
    );

    Serial.println("[SIM7600] UART initialise");

    delay(1000);

    for (
        int attempt = 1;
        attempt <= SIM7600_AT_ATTEMPTS;
        attempt++
    )
    {
        Serial.print("[SIM7600] Test AT ");
        Serial.print(attempt);
        Serial.print("/");
        Serial.println(SIM7600_AT_ATTEMPTS);

        if (testAT())
        {
            _ready = true;
            Serial.println("[SIM7600] MODEM READY");
            return true;
        }

        delay(1000);
    }

    _ready = false;
    Serial.println("[SIM7600] MODEM NOT READY");

    return false;
}


// ============================================================
// SEND COMMAND
// ============================================================

String SIM7600Driver::sendCommand(
    const String& command,
    uint32_t timeout
)
{
    if (_serial == nullptr)
    {
        return "";
    }

    while (_serial->available())
    {
        _serial->read();
    }

    Serial.print("[SIM7600 TX] ");
    Serial.println(command);

    _serial->println(command);

    String response = "";
    uint32_t startTime = millis();
    bool finished = false;

    while (millis() - startTime < timeout)
    {
        while (_serial->available())
        {
            char c = _serial->read();
            response += c;

            if (
                response.indexOf("\r\nOK\r\n") >= 0 ||
                response.indexOf("\nOK\n") >= 0 ||
                response.indexOf("\r\nERROR\r\n") >= 0 ||
                response.indexOf("\nERROR\n") >= 0
            )
            {
                finished = true;
            }
        }

        if (finished)
        {
            break;
        }

        delay(10);
    }

    Serial.println("[SIM7600 RX]");
    Serial.println(response);

    return response;
}


// ============================================================
// TEST AT
// ============================================================

bool SIM7600Driver::testAT()
{
    String response = sendCommand(
        "AT",
        SIM7600_AT_TIMEOUT_MS
    );

    return response.indexOf("OK") >= 0;
}


// ============================================================
// MODEM INFORMATION
// ============================================================

bool SIM7600Driver::getModemInformation()
{
    String response = sendCommand(
        "ATI",
        SIM7600_INFO_TIMEOUT_MS
    );

    return response.indexOf("OK") >= 0;
}


// ============================================================
// TEST SIM
// ============================================================

bool SIM7600Driver::testSIM()
{
    String response = sendCommand(
        "AT+CPIN?",
        SIM7600_INFO_TIMEOUT_MS
    );

    return response.indexOf("+CPIN: READY") >= 0 ||
           response.indexOf("OK") >= 0;
}


// ============================================================
// TEST SIGNAL
// ============================================================

bool SIM7600Driver::testSignal()
{
    String response = sendCommand(
        "AT+CSQ",
        SIM7600_INFO_TIMEOUT_MS
    );

    return response.indexOf("+CSQ:") >= 0;
}


// ============================================================
// TEST NETWORK
// ============================================================

bool SIM7600Driver::testNetwork()
{
    String response = sendCommand(
        "AT+CREG?",
        SIM7600_INFO_TIMEOUT_MS
    );

    return response.indexOf("+CREG: 0,1") >= 0 ||
           response.indexOf("+CREG: 1") >= 0 ||
           response.indexOf("+CREG: 0,5") >= 0;
}


// ============================================================
// TEST OPERATOR
// ============================================================

bool SIM7600Driver::testOperator()
{
    String response = sendCommand(
        "AT+COPS?",
        SIM7600_INFO_TIMEOUT_MS
    );

    return response.indexOf("+COPS:") >= 0;
}


// ============================================================
// TEST CPSI
// ============================================================

bool SIM7600Driver::testCPSI()
{
    String response = sendCommand(
        "AT+CPSI?",
        SIM7600_INFO_TIMEOUT_MS
    );

    return response.indexOf("+CPSI:") >= 0;
}


// ============================================================
// TEST PACKET ATTACH
// ============================================================

bool SIM7600Driver::testPacketAttach()
{
    String response = sendCommand(
        "AT+CGATT?",
        SIM7600_INFO_TIMEOUT_MS
    );

    return response.indexOf("+CGATT: 1") >= 0;
}


// ============================================================
// WAIT FOR NETWORK
// ============================================================

bool SIM7600Driver::waitForNetwork()
{
    for (int i = 0; i < 10; i++)
    {
        String response = sendCommand(
            "AT+CREG?",
            SIM7600_NETWORK_TIMEOUT_MS
        );

        if (
            response.indexOf("+CREG: 0,1") >= 0 ||
            response.indexOf("+CREG: 0,5") >= 0 ||
            response.indexOf("+CREG: 1") >= 0
        )
        {
            Serial.println("[SIM7600] RESEAU ENREGISTRE");
            return true;
        }

        delay(2000);
    }

    return false;
}


// ============================================================
// CONFIGURE APN
// ============================================================

bool SIM7600Driver::configureAPN(
    const String& apn
)
{
    String command = "AT+CGDCONT=1,\"IP\",\"" + apn + "\"";

    String response = sendCommand(
        command,
        SIM7600_INFO_TIMEOUT_MS
    );

    if (response.indexOf("OK") >= 0)
    {
        Serial.print("[INTERNET] APN : ");
        Serial.println(apn);
        return true;
    }

    Serial.println("[INTERNET] APN FAILED");
    return false;
}


// ============================================================
// ACTIVATE DATA
// ============================================================

bool SIM7600Driver::activateData()
{
    String attachResponse = sendCommand(
        "AT+CGATT?",
        SIM7600_INFO_TIMEOUT_MS
    );

    if (attachResponse.indexOf("+CGATT: 1") < 0)
    {
        String setAttach = sendCommand(
            "AT+CGATT=1",
            15000
        );

        if (setAttach.indexOf("OK") < 0)
        {
            return false;
        }
    }

    String activate = sendCommand(
        "AT+CGACT=1,1",
        15000
    );

    if (activate.indexOf("OK") < 0)
    {
        return false;
    }

    _dataConnected = true;

    Serial.println("[INTERNET] DATA INTERNET READY");
    return true;
}


// ============================================================
// GET IP ADDRESS
// ============================================================

bool SIM7600Driver::getIPAddress(
    String& ipAddress
)
{
    String response = sendCommand(
        "AT+CGPADDR=1",
        SIM7600_INFO_TIMEOUT_MS
    );

    int pos = response.indexOf("+CGPADDR:");

    if (pos < 0)
    {
        ipAddress = "";
        return false;
    }

    int comma = response.indexOf(',', pos);

    if (comma < 0)
    {
        ipAddress = "";
        return false;
    }

    int start = comma + 1;
    while (start < response.length() && response[start] == ' ')
    {
        start++;
    }

    int end = response.indexOf('\r', start);
    if (end < 0)
    {
        end = response.indexOf('\n', start);
    }
    if (end < 0)
    {
        end = response.length();
    }

    ipAddress = response.substring(start, end);
    ipAddress.trim();

    return ipAddress.length() > 0;
}


// ============================================================
// TEST INTERNET
// ============================================================

bool SIM7600Driver::testInternet()
{
    String response = sendCommand(
        "AT+CPING=\"8.8.8.8\",1,4,64,10000,10000,255",
        15000
    );

    return response.indexOf("OK") >= 0 ||
           response.indexOf("+CPING:") >= 0;
}


// ============================================================
// INTERNET CONNECTED
// ============================================================

bool SIM7600Driver::isInternetConnected()
{
    return _dataConnected;
}
HardwareSerial& SIM7600Driver::getSerial()
{
    if (_serial == nullptr)
    {
        _serial = new HardwareSerial(
            SIM7600_UART_NUMBER
        );
    }

    return *_serial;
}

// ============================================================
// GPS ENABLE
// ============================================================

bool SIM7600Driver::gpsEnable()
{
    Serial.println("[GPS] Activation GPS...");

    /*
     * On regarde d'abord si le GPS est déjà actif.
     *
     * Cela évite d'envoyer inutilement :
     *
     * AT+CGPS=1
     *
     * plusieurs fois.
     */

    String status = sendCommand(
        "AT+CGPS?",
        5000
    );

    if (status.indexOf("+CGPS: 1") >= 0)
    {
        Serial.println("[GPS] GPS deja actif");
        return true;
    }


    // --------------------------------------------------------
    // GPS actuellement OFF
    // --------------------------------------------------------

    String response = sendCommand(
        "AT+CGPS=1",
        10000
    );

    if (response.indexOf("OK") < 0)
    {
        Serial.println("[GPS] ECHEC ACTIVATION");
        return false;
    }

    Serial.println("[GPS] GPS ACTIVE");

    return true;
}


// ============================================================
// GPS DISABLE
// ============================================================

bool SIM7600Driver::gpsDisable()
{
    Serial.println("[GPS] Desactivation GPS...");

    String response = sendCommand(
        "AT+CGPS=0",
        5000
    );

    if (response.indexOf("OK") < 0)
    {
        Serial.println("[GPS] ECHEC DESACTIVATION");
        return false;
    }

    Serial.println("[GPS] GPS OFF");

    return true;
}


// ============================================================
// GPS STATUS
// ============================================================

bool SIM7600Driver::gpsGetStatus()
{
    Serial.println("[GPS] Verification statut...");

    String response = sendCommand(
        "AT+CGPS?",
        5000
    );

    if (response.indexOf("+CGPS: 1") >= 0)
    {
        Serial.println("[GPS] GPS ON");
        return true;
    }

    if (response.indexOf("+CGPS: 0") >= 0)
    {
        Serial.println("[GPS] GPS OFF");
        return false;
    }

    Serial.println("[GPS] Statut GPS inconnu");

    return false;
}


// ============================================================
// GPS RAW INFO
// ============================================================

bool SIM7600Driver::gpsGetRawInfo()
{
    Serial.println();
    Serial.println("[GPS] Lecture AT+CGPSINFO...");

    String response = sendCommand(
        "AT+CGPSINFO",
        10000
    );

    Serial.println();

    Serial.println("[GPS RAW] REPONSE COMPLETE");
    Serial.println("-----------------------------");

    Serial.print(response);

    Serial.println("-----------------------------");


    if (response.indexOf("+CGPSINFO:") >= 0)
    {
        return true;
    }

    Serial.println("[GPS] +CGPSINFO ABSENT");

    return false;
}