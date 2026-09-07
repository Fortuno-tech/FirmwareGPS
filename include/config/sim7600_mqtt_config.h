#pragma once

namespace fortico {
namespace modem_mqtt {

/**
 * Gabarits des commandes MQTT AT du SIM7600.
 * Les paramètres dynamiques sont injectés avec snprintf() par l'adaptateur.
 */
struct Commands {
    static constexpr const char* START = "AT+CMQTTSTART";
    static constexpr const char* ACQUIRE_CLIENT =
        "AT+CMQTTACCQ=0,\"%s\",0";
    static constexpr const char* CONNECT =
        "AT+CMQTTCONNECT=0,\"tcp://%s:%u\",%u,1,\"%s\",\"%s\"";
    static constexpr const char* SET_TOPIC_LENGTH =
        "AT+CMQTTTOPIC=0,%u";
    static constexpr const char* SET_PAYLOAD_LENGTH =
        "AT+CMQTTPAYLOAD=0,%u";
    static constexpr const char* PUBLISH = "AT+CMQTTPUB=0,%u,%u";
    static constexpr const char* DISCONNECT = "AT+CMQTTDISC=0,%u";
    static constexpr const char* STOP = "AT+CMQTTSTOP";

    static constexpr unsigned int CLIENT_INDEX = 0U;
    static constexpr unsigned int KEEPALIVE_SECONDS = 60U;
    static constexpr unsigned int PUBLISH_QOS = 1U;
    static constexpr unsigned int PUBLISH_TIMEOUT_SECONDS = 60U;
};

}  // namespace modem_mqtt
}  // namespace fortico