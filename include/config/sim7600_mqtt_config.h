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
        "AT+CMQTTACCQ=0,\"%s\",1,4";
    static constexpr const char* CONFIGURE_TLS_VERSION =
        "AT+CSSLCFG=\"sslversion\",0,3";
    static constexpr const char* CONFIGURE_TLS_AUTH_MODE =
        "AT+CSSLCFG=\"authmode\",0,1";
    static constexpr const char* CONFIGURE_TLS_CA =
        "AT+CSSLCFG=\"cacert\",0,\"%s\"";
    static constexpr const char* CONFIGURE_TLS_IGNORE_LOCAL_TIME =
        "AT+CSSLCFG=\"ignorelocaltime\",0,1";
    static constexpr const char* CONFIGURE_TLS_SNI =
        "AT+CSSLCFG=\"enableSNI\",0,1";
    static constexpr const char* ASSIGN_TLS_CONTEXT =
        "AT+CMQTTSSLCFG=0,0";
    static constexpr const char* CONNECT =
        "AT+CMQTTCONNECT=0,\"tcp://%s:%u\",%u,1,\"%s\",\"%s\"";
    static constexpr const char* SET_TOPIC_LENGTH =
        "AT+CMQTTTOPIC=0,%u";
    static constexpr const char* SET_PAYLOAD_LENGTH =
        "AT+CMQTTPAYLOAD=0,%u";
    static constexpr const char* PUBLISH = "AT+CMQTTPUB=0,%u,%u";
    static constexpr const char* DISCONNECT = "AT+CMQTTDISC=0,%u";
    static constexpr const char* RELEASE_CLIENT = "AT+CMQTTREL=0";
    static constexpr const char* STOP = "AT+CMQTTSTOP";

    static constexpr const char* LIST_CERTIFICATES = "AT+CCERTLIST";

    static constexpr unsigned int CLIENT_INDEX = 0U;
    static constexpr unsigned int KEEPALIVE_SECONDS = 60U;
    static constexpr unsigned int PUBLISH_QOS = 1U;
    static constexpr unsigned int PUBLISH_TIMEOUT_SECONDS = 60U;

    /// Nom du certificat CA PEM Ã  provisionner dans le stockage du SIM7600.
    /// Ce nom, et non le certificat, est une configuration non secrÃ¨te.
    static constexpr const char* HIVE_MQ_CA_CERTIFICATE_FILE = "hivemq-ca.pem";
};

}  // namespace modem_mqtt
}  // namespace fortico
