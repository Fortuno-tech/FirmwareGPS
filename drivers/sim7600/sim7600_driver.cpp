#include "sim7600_driver.h"

#include <Arduino.h>

#include "pin_config.h"
#include "hardware_config.h"
#include "uart_hal.h"


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
    Serial.println("     SIM7600 DRIVER");
    Serial.println("=================================");

    // Création UART2
    _serial = new HardwareSerial(
        SIM7600_UART_NUMBER
    );

    // Initialisation UART
    UARTHAL::begin(
        *_serial,
        SIM7600_BAUDRATE,
        SIM7600_RX_PIN,
        SIM7600_TX_PIN
    );

    Serial.println(
        "UART SIM7600 initialise."
    );

    // --------------------------------------------------------
    // IMPORTANT :
    // Le SIM7600 peut encore être en train de démarrer.
    // --------------------------------------------------------

    Serial.println(
        "Attente demarrage modem..."
    );

    delay(3000);

    // --------------------------------------------------------
    // Test AT plusieurs fois
    // --------------------------------------------------------

    for (
        int attempt = 1;
        attempt <= SIM7600_MAX_AT_ATTEMPTS;
        attempt++
    )
    {
        Serial.print(
            "Test AT "
        );

        Serial.print(attempt);

        Serial.print(
            "/"
        );

        Serial.print(
            SIM7600_MAX_AT_ATTEMPTS
        );

        Serial.println(
            "..."
        );

        if (testAT())
        {
            Serial.println(
                "[SIM7600] AT : OK"
            );

            _ready = true;

            return true;
        }

        Serial.println(
            "[SIM7600] Pas encore pret."
        );

        delay(2000);
    }

    Serial.println(
        "[SIM7600] AT : FAILED"
    );

    _ready = false;

    return false;
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
// INFORMATIONS MODEM
// ============================================================

bool SIM7600Driver::getModemInformation(
    String& information
)
{
    information = sendCommand(
        "ATI",
        SIM7600_INFO_TIMEOUT_MS
    );

    if (information.indexOf("SIMCOM") >= 0)
    {
        Serial.println(
            "[SIM7600] MODEM : OK"
        );

        Serial.println(
            information
        );

        return true;
    }

    Serial.println(
        "[SIM7600] MODEM : FAILED"
    );

    return false;
}


// ============================================================
// TEST SIM
// ============================================================

bool SIM7600Driver::testSIM()
{
    String response = sendCommand(
        "AT+CPIN?",
        SIM7600_AT_TIMEOUT_MS
    );

    Serial.println(
        response
    );

    if (
        response.indexOf(
            "+CPIN: READY"
        ) >= 0
    )
    {
        Serial.println(
            "[SIM7600] SIM : READY"
        );

        return true;
    }

    Serial.println(
        "[SIM7600] SIM : FAILED"
    );

    return false;
}


// ============================================================
// TEST SIGNAL
// ============================================================

bool SIM7600Driver::testSignal()
{
    String response = sendCommand(
        "AT+CSQ",
        SIM7600_AT_TIMEOUT_MS
    );

    Serial.println(
        response
    );

    int index = response.indexOf(
        "+CSQ:"
    );

    if (index < 0)
    {
        Serial.println(
            "[SIM7600] SIGNAL : INCONNU"
        );

        return false;
    }

    int comma = response.indexOf(
        ',',
        index
    );

    if (comma < 0)
    {
        return false;
    }

    String rssiString = response.substring(
        index + 5,
        comma
    );

    rssiString.trim();

    int rssi = rssiString.toInt();

    Serial.print(
        "[SIM7600] RSSI = "
    );

    Serial.println(
        rssi
    );

    // 99 = signal inconnu
    if (rssi == 99)
    {
        Serial.println(
            "[SIM7600] SIGNAL : INDISPONIBLE"
        );

        return false;
    }

    if (rssi >= 0 && rssi <= 31)
    {
        Serial.println(
            "[SIM7600] SIGNAL : DISPONIBLE"
        );

        return true;
    }

    return false;
}


// ============================================================
// TEST RESEAU
// ============================================================

bool SIM7600Driver::testNetwork()
{
    String response = sendCommand(
        "AT+CREG?",
        SIM7600_NETWORK_TIMEOUT_MS
    );

    Serial.println(
        response
    );

    if (
        isNetworkRegistered(
            response
        )
    )
    {
        Serial.println(
            "[SIM7600] RESEAU : ENREGISTRE"
        );

        return true;
    }

    Serial.println(
        "[SIM7600] RESEAU : NON ENREGISTRE"
    );

    return false;
}


// ============================================================
// TEST OPERATEUR
// ============================================================

bool SIM7600Driver::testOperator()
{
    String response = sendCommand(
        "AT+COPS?",
        SIM7600_INFO_TIMEOUT_MS
    );

    Serial.println(
        response
    );

    if (
        response.indexOf(
            "+COPS:"
        ) >= 0
    )
    {
        Serial.println(
            "[SIM7600] OPERATEUR : OK"
        );

        return true;
    }

    Serial.println(
        "[SIM7600] OPERATEUR : FAILED"
    );

    return false;
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

    Serial.println(
        response
    );

    if (
        response.indexOf(
            "NO SERVICE"
        ) >= 0
    )
    {
        Serial.println(
            "[SIM7600] CPSI : NO SERVICE"
        );

        return false;
    }

    if (
        response.indexOf(
            "+CPSI:"
        ) >= 0
    )
    {
        Serial.println(
            "[SIM7600] CPSI : OK"
        );

        return true;
    }

    Serial.println(
        "[SIM7600] CPSI : FAILED"
    );

    return false;
}


// ============================================================
// TEST ATTACH DATA
// ============================================================

bool SIM7600Driver::testPacketAttach()
{
    String response = sendCommand(
        "AT+CGATT?",
        SIM7600_AT_TIMEOUT_MS
    );

    Serial.println(
        response
    );

    if (
        response.indexOf(
            "+CGATT: 1"
        ) >= 0
    )
    {
        Serial.println(
            "[SIM7600] DATA : ATTACHE"
        );

        return true;
    }

    Serial.println(
        "[SIM7600] DATA : NON ATTACHE"
    );

    return false;
}


// ============================================================
// ATTENTE RESEAU
// ============================================================

bool SIM7600Driver::waitForNetwork(
    uint32_t timeoutMs
)
{
    Serial.println();
    Serial.println(
        "================================="
    );

    Serial.println(
        "     RECHERCHE RESEAU"
    );

    Serial.println(
        "================================="
    );

    uint32_t startTime = millis();

    while (
        millis() - startTime < timeoutMs
    )
    {
        Serial.println();
        Serial.println(
            "[RESEAU] Verification..."
        );

        // ----------------------------------------------------
        // CREG
        // ----------------------------------------------------

        String creg = sendCommand(
            "AT+CREG?",
            3000
        );

        Serial.print(
            "[CREG] "
        );

        Serial.println(
            creg
        );

        if (
            isNetworkRegistered(
                creg
            )
        )
        {
            Serial.println(
                "[RESEAU] ENREGISTRE !"
            );

            // ------------------------------------------------
            // CPSI
            // ------------------------------------------------

            String cpsi = sendCommand(
                "AT+CPSI?",
                5000
            );

            Serial.print(
                "[CPSI] "
            );

            Serial.println(
                cpsi
            );

            if (
                isServiceAvailable(
                    cpsi
                )
            )
            {
                Serial.println();
                Serial.println(
                    "================================="
                );

                Serial.println(
                    "     RESEAU DISPONIBLE"
                );

                Serial.println(
                    "================================="
                );

                return true;
            }
        }
        else
        {
            // ------------------------------------------------
            // CSQ pendant recherche
            // ------------------------------------------------

            String csq = sendCommand(
                "AT+CSQ",
                3000
            );

            Serial.print(
                "[CSQ] "
            );

            Serial.println(
                csq
            );

            Serial.println(
                "[RESEAU] Toujours en recherche..."
            );
        }

        delay(
            SIM7600_NETWORK_CHECK_INTERVAL_MS
        );
    }

    Serial.println();
    Serial.println(
        "[RESEAU] TIMEOUT"
    );

    return false;
}


// ============================================================
// CONFIGURATION APN
// ============================================================

bool SIM7600Driver::configureAPN(
    const String& apn
)
{
    if (apn.length() == 0)
    {
        Serial.println(
            "[SIM7600] APN VIDE"
        );

        return false;
    }

    String command =
        "AT+CGDCONT=";

    command += String(
        SIM7600_PDP_CONTEXT
    );

    command +=
        ",\"IP\",\"";

    command += apn;

    command += "\"";

    String response = sendCommand(
        command,
        5000
    );

    Serial.println(
        response
    );

    if (
        response.indexOf(
            "OK"
        ) >= 0
    )
    {
        Serial.println(
            "[SIM7600] APN : OK"
        );

        return true;
    }

    Serial.println(
        "[SIM7600] APN : FAILED"
    );

    return false;
}


// ============================================================
// ACTIVATION DATA
// ============================================================

bool SIM7600Driver::activateData()
{
    Serial.println();
    Serial.println(
        "Activation connexion DATA..."
    );

    // --------------------------------------------------------
    // Vérifier CGATT
    // --------------------------------------------------------

    String attach = sendCommand(
        "AT+CGATT?",
        5000
    );

    Serial.println(
        attach
    );

    if (
        attach.indexOf(
            "+CGATT: 1"
        ) < 0
    )
    {
        Serial.println(
            "Packet attach necessaire..."
        );

        String response = sendCommand(
            "AT+CGATT=1",
            30000
        );

        Serial.println(
            response
        );

        if (
            response.indexOf(
                "OK"
            ) < 0
        )
        {
            Serial.println(
                "[SIM7600] CGATT FAILED"
            );

            return false;
        }
    }

    // --------------------------------------------------------
    // Activation PDP
    // --------------------------------------------------------

    Serial.println(
        "Activation PDP context..."
    );

    String response = sendCommand(
        "AT+CGACT=1,1",
        30000
    );

    Serial.println(
        response
    );

    if (
        response.indexOf(
            "OK"
        ) >= 0
    )
    {
        _dataConnected = true;

        Serial.println(
            "[SIM7600] DATA : CONNECTEE"
        );

        return true;
    }

    _dataConnected = false;

    Serial.println(
        "[SIM7600] DATA : FAILED"
    );

    return false;
}


// ============================================================
// GET IP
// ============================================================

bool SIM7600Driver::getIPAddress(
    String& ipAddress
)
{
    ipAddress = "";

    String response = sendCommand(
        "AT+CGPADDR=1",
        5000
    );

    Serial.println(
        response
    );

    int index = response.indexOf(
        "+CGPADDR:"
    );

    if (index < 0)
    {
        Serial.println(
            "[SIM7600] IP : ABSENTE"
        );

        return false;
    }

    int comma = response.indexOf(
        ',',
        index
    );

    if (comma < 0)
    {
        return false;
    }

    int end = response.indexOf(
        '\r',
        comma
    );

    if (end < 0)
    {
        end = response.length();
    }

    ipAddress = response.substring(
        comma + 1,
        end
    );

    ipAddress.trim();

    if (
        ipAddress.length() == 0 ||
        ipAddress == "0.0.0.0"
    )
    {
        Serial.println(
            "[SIM7600] IP INVALIDE"
        );

        return false;
    }

    Serial.print(
        "[SIM7600] IP = "
    );

    Serial.println(
        ipAddress
    );

    return true;
}


// ============================================================
// TEST INTERNET
// ============================================================

bool SIM7600Driver::testInternet()
{
    String ip;

    if (
        !getIPAddress(ip)
    )
    {
        Serial.println(
            "[SIM7600] INTERNET : PAS D'IP"
        );

        return false;
    }

    Serial.println(
        "[SIM7600] INTERNET : IP OBTENUE"
    );

    return true;
}


// ============================================================
// ETAT DATA
// ============================================================

bool SIM7600Driver::isDataConnected() const
{
    return _dataConnected;
}


// ============================================================
// ETAT MODEM
// ============================================================

bool SIM7600Driver::isReady() const
{
    return _ready;
}


// ============================================================
// SEND COMMAND
// ============================================================

String SIM7600Driver::sendCommand(
    const String& command,
    uint32_t timeout
)
{
    if (
        _serial == nullptr
    )
    {
        return "";
    }

    // Nettoyage buffer
    UARTHAL::flushInput(
        *_serial
    );

    Serial.print(
        "[SIM7600 TX] "
    );

    Serial.println(
        command
    );

    // Envoi commande
    _serial->print(
        command
    );

    _serial->print(
        "\r\n"
    );

    String response = "";

    uint32_t start = millis();

    while (
        millis() - start < timeout
    )
    {
        while (
            _serial->available()
        )
        {
            char c =
                _serial->read();

            response += c;

            // Fin OK
            if (
                response.indexOf(
                    "\r\nOK"
                ) >= 0
            )
            {
                return response;
            }

            // Fin ERROR
            if (
                response.indexOf(
                    "\r\nERROR"
                ) >= 0
            )
            {
                return response;
            }
        }

        delay(1);
    }

    return response;
}


// ============================================================
// HELPERS
// ============================================================

bool SIM7600Driver::isNetworkRegistered(
    const String& response
)
{
    // Enregistrement local
    if (
        response.indexOf(
            "+CREG: 0,1"
        ) >= 0
    )
    {
        return true;
    }

    if (
        response.indexOf(
            "+CREG: 1,1"
        ) >= 0
    )
    {
        return true;
    }

    // Roaming
    if (
        response.indexOf(
            "+CREG: 0,5"
        ) >= 0
    )
    {
        return true;
    }

    if (
        response.indexOf(
            "+CREG: 1,5"
        ) >= 0
    )
    {
        return true;
    }

    return false;
}


bool SIM7600Driver::isSignalAvailable(
    const String& response
)
{
    return (
        response.indexOf(
            "+CSQ:"
        ) >= 0 &&
        response.indexOf(
            "+CSQ: 99"
        ) < 0
    );
}


bool SIM7600Driver::isServiceAvailable(
    const String& response
)
{
    if (
        response.indexOf(
            "NO SERVICE"
        ) >= 0
    )
    {
        return false;
    }

    return (
        response.indexOf(
            "+CPSI:"
        ) >= 0
    );
}