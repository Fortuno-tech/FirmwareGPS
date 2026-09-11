#include "drivers/rfid/rfid_driver.h"

#include <Arduino.h>
#include <cstring>

#include "config/hardware_config.h"
#include "config/pin_config.h"
#include "config/task_config.h"

namespace fortico {
namespace drivers {

/* ============================================================
 * CONSTRUCTEUR
 * ============================================================ */

RfidDriver::RfidDriver(hal::I2cHal& i2cHal)
    : _i2cHal(i2cHal)

#ifdef ESP32_TARGET
    , _pn532(
        pins::PN532_IRQ,
        pins::PN532_RESET,
        &i2cHal.native())
    , _taskHandle(nullptr)
    , _resultMutex(nullptr)
    , _taskRunning(false)
    , _taskStarted(false)
    , _passiveDetectionStarted(false)
    , _awaitingDetectionAck(false)
    , _irqPending(false)
    , _irqAttached(false)
    , _lastDetectionError(0U)
    , _detectionStartCount(0U)
    , _irqWaitCount(0U)
    , _responseReadCount(0U)
    , _lastReadAttemptMs(0U)
    , _resultUid{}
    , _resultUidLength(0U)
    , _resultAvailable(false)
#endif

    , _initialized(false)
    , _healthy(false)
    , _active(false)
{
}


/* ============================================================
 * ATTENDRE LA DÉTECTION DU PN532 SUR I2C
 * ============================================================ */

bool RfidDriver::waitForPn532Availability(
    uint32_t timeoutMs,
    uint32_t pollIntervalMs)
{
    const uint32_t startTimeMs = millis();
    TwoWire& wire = _i2cHal.native();

    wire.setClock(hardware::I2C_FREQUENCY_HZ);
    wire.setTimeOut(PN532_I2C_TRANSACTION_TIMEOUT_MS);

    while ((millis() - startTimeMs) < timeoutMs) {

        wire.beginTransmission(RFID_PN532_I2C_ADDRESS);

        if (wire.endTransmission() == 0U) {
            return true;
        }

        vTaskDelay(
            pdMS_TO_TICKS(pollIntervalMs));
    }

    return false;
}


/* ============================================================
 * CONFIGURATION DES BROCHES PN532
 * ============================================================ */

void RfidDriver::configurePins()
{
    pinMode(
        pins::PN532_RESET,
        OUTPUT);

    pinMode(
        pins::PN532_IRQ,
        INPUT);

    digitalWrite(
        pins::PN532_RESET,
        HIGH);
}


void IRAM_ATTR RfidDriver::irqHandler(void* context)
{
    RfidDriver* const driver = static_cast<RfidDriver*>(context);
    if (driver != nullptr) {
        driver->_irqPending = true;
    }
}


/* ============================================================
 * RESET HARDWARE PN532
 * ============================================================ */

void RfidDriver::performHardwareReset()
{
    configurePins();

    digitalWrite(
        pins::PN532_RESET,
        LOW);

    vTaskDelay(
        pdMS_TO_TICKS(PN532_RESET_ASSERT_MS));

    digitalWrite(
        pins::PN532_RESET,
        HIGH);

    vTaskDelay(
        pdMS_TO_TICKS(PN532_RESET_HIGH_MS));
}


/* ============================================================
 * RÉGLAGE I2C PN532
 * ============================================================ */

void RfidDriver::applyPn532I2cSettings()
{
    _i2cHal.native().setClock(
        hardware::I2C_FREQUENCY_HZ);

    _i2cHal.native().setTimeOut(
        PN532_I2C_TRANSACTION_TIMEOUT_MS);
}


/* ============================================================
 * CONFIGURATION PN532
 * ============================================================ */

bool RfidDriver::configurePn532()
{
    if (!_i2cHal.lock(
            hardware::I2C_MUTEX_TIMEOUT_MS)) {

        Serial.println(
            "[RFID][ERROR] Mutex I2C indisponible");

        return false;
    }

    applyPn532I2cSettings();

    /*
     * Initialisation PN532
     */
    const bool started =
        _pn532.begin();

    if (!started) {

        _i2cHal.unlock();

        Serial.println(
            "[RFID][ERROR] PN532 begin() echoue");

        return false;
    }

    /*
     * Lecture firmware
     */
    applyPn532I2cSettings();

    const uint32_t firmwareVersion =
        _pn532.getFirmwareVersion();

    if (firmwareVersion == 0U) {

        _i2cHal.unlock();

        Serial.println(
            "[RFID][ERROR] Firmware PN532 non detecte");

        return false;
    }

    Serial.printf(
        "[RFID] PN532 firmware: 0x%08lX\n",
        static_cast<unsigned long>(
            firmwareVersion));

    /*
     * Configuration SAM
     */
    applyPn532I2cSettings();

    const bool samConfigured =
        _pn532.SAMConfig();

    if (!samConfigured) {

        _i2cHal.unlock();

        Serial.println(
            "[RFID][ERROR] SAMConfig() echoue");

        return false;
    }

    /*
     * Nombre de tentatives de détection.
     *
     * 0x20 = 32 tentatives.
     *
     * Cela laisse davantage de temps au badge
     * pour être présenté devant le lecteur.
     */
    applyPn532I2cSettings();

    _i2cHal.unlock();

    Serial.println(
        "[RFID] PN532 configure avec succes");

    return true;
}


/* ============================================================
 * LANCER UNE DETECTION PASSIVE SANS ATTENDRE UNE CARTE
 * ============================================================ */

bool RfidDriver::startPassiveTargetDetection()
{
    const uint8_t previousDetectionError = _lastDetectionError;
    _lastDetectionError = 0U;

    Serial.println(
        "[RFID][DBG] startPassiveTargetDetection: begin command emission");

    // Re-synchronise le contrôleur RF après un timeout ou un état PN532
    // incohérent. L'appel reste sous le mutex acquis par readHardwareUid().
    if (previousDetectionError == PN532_INVALID_STATE_ERROR) {
        if (!_pn532.SAMConfig() ||
            !_pn532.setPassiveActivationRetries(
                PN532_PASSIVE_ACTIVATION_RETRIES)) {
            _lastDetectionError = 50U;
            return false;
        }
    }

    // InListPassiveTarget: max one ISO14443A target.
    constexpr uint8_t command[] = {0x4AU, 0x01U, 0x00U};
    constexpr uint8_t payloadLength = sizeof(command) + 1U;
    uint8_t frame[8U + sizeof(command)]{};

    frame[0] = 0x00U;
    frame[1] = 0x00U;
    frame[2] = 0xFFU;
    frame[3] = payloadLength;
    frame[4] = static_cast<uint8_t>(~payloadLength + 1U);
    frame[5] = 0xD4U;

    uint8_t checksum = 0xD4U;
    for (uint8_t index = 0U; index < sizeof(command); ++index) {
        frame[6U + index] = command[index];
        checksum = static_cast<uint8_t>(checksum + command[index]);
    }

    frame[6U + sizeof(command)] =
        static_cast<uint8_t>(~checksum + 1U);
    frame[7U + sizeof(command)] = 0x00U;

    TwoWire& wire = _i2cHal.native();
    wire.setTimeOut(PN532_COMMAND_TIMEOUT_MS);
    wire.beginTransmission(RFID_PN532_I2C_ADDRESS);
    if (wire.write(frame, sizeof(frame)) != sizeof(frame)) {
        _lastDetectionError = 1U;
        wire.setTimeOut(hardware::I2C_TRANSACTION_MAX_MS);
        Serial.println(
            "[RFID][DBG] startPassiveTargetDetection: write(frame) failed");
        return false;
    }

    const uint8_t transmissionError = wire.endTransmission();
    // Le PN532 peut maintenir SCL bas pendant le traitement de la commande.
    // Arduino-ESP32 remonte alors 5 (timeout) alors que la trame a deja ete
    // recue; Adafruit_I2CDevice ignore egalement ce retour dans writecommand.
    // L'IRQ confirmera ensuite l'ACK ou la reponse finale.
    if (transmissionError != 0U && transmissionError != 5U) {
        _lastDetectionError = static_cast<uint8_t>(10U + transmissionError);
        if (_lastDetectionError == PN532_INVALID_STATE_ERROR) {
            _passiveDetectionStarted = false;
        }
        wire.setTimeOut(hardware::I2C_TRANSACTION_MAX_MS);
        Serial.printf(
            "[RFID][DBG] startPassiveTargetDetection: endTransmission failed, error=%u\n",
            static_cast<unsigned int>(transmissionError));
        return false;
    }

    wire.setTimeOut(hardware::I2C_TRANSACTION_MAX_MS);

    Serial.println(
        "[RFID][DBG] startPassiveTargetDetection: command sent successfully, waiting for PN532 response");

    // The PN532 now scans autonomously; the next I2C read is IRQ-gated.
    return true;
}


/* ============================================================
 * LIRE UNE REPONSE PN532 DE LONGUEUR VARIABLE
 * ============================================================ */

bool RfidDriver::readPassiveTargetResponse(
    uint8_t* uid,
    uint8_t& uidLength)
{
    uidLength = 0U;

    TwoWire& wire = _i2cHal.native();
    uint8_t response[21U]{};
    const size_t responseSize = wire.requestFrom(
        static_cast<int>(RFID_PN532_I2C_ADDRESS),
        static_cast<int>(sizeof(response)));

    if (responseSize != sizeof(response)) {
        Serial.printf(
            "[RFID][DBG] readPassiveTargetResponse: request failed, requested=%zu received=%zu\n",
            sizeof(response), responseSize);
        return false;
    }

    for (uint8_t index = 0U; index < sizeof(response); ++index) {
        response[index] = static_cast<uint8_t>(wire.read());
    }

    // Le premier IRQ est l'ACK de commande; l'IRQ suivant contient le resultat.
    if (_awaitingDetectionAck && response[0] == 0x01U &&
        response[1] == 0x00U && response[2] == 0x00U &&
        response[3] == 0xFFU && response[4] == 0x00U &&
        response[5] == 0xFFU && response[6] == 0x00U) {
        _awaitingDetectionAck = false;
        return false;
    }

    // response[0] est le statut I2C; la trame PN532 commence a response[1].
    if (response[0] != 0x01U || response[1] != 0x00U ||
        response[2] != 0x00U || response[3] != 0xFFU) {
        return false;
    }

    const uint8_t payloadLength = response[4];
    if (payloadLength < 2U ||
        response[5] != static_cast<uint8_t>(~payloadLength + 1U) ||
        static_cast<size_t>(payloadLength) + 8U > sizeof(response) ||
        response[6] != 0xD5U || response[7] != 0x4BU) {
        return false;
    }

    // NbTg=0 est une reponse valide sans badge.
    if (response[8] == 0U) {
        return true;
    }

    const uint8_t detectedUidLength = response[13];
    if (response[8] != 1U || detectedUidLength == 0U ||
        detectedUidLength > 16U ||
        static_cast<size_t>(14U + detectedUidLength) > sizeof(response) ||
        uid == nullptr) {
        return false;
    }

    uidLength = detectedUidLength;
    for (uint8_t index = 0U; index < uidLength; ++index) {
        uid[index] = response[14U + index];
    }

    Serial.printf(
        "[RFID] Reponse PN532: cible=%u uidLength=%u\n",
        static_cast<unsigned int>(response[8]),
        static_cast<unsigned int>(uidLength));

    return true;
}


/* ============================================================
 * INITIALISATION
 * ============================================================ */

constants::ErrorCode RfidDriver::init()
{
    if (_initialized) {
        return constants::ErrorCode::OK;
    }

#ifdef ESP32_TARGET

    Serial.println(
        "[RFID] Initialisation I2C...");

    if (!_i2cHal.init(
            pins::I2C_SDA,
            pins::I2C_SCL,
            hardware::I2C_FREQUENCY_HZ)) {

        Serial.println(
            "[RFID][ERROR] Initialisation I2C echouee");

        return constants::ErrorCode::I2C_BUS_TIMEOUT;
    }

    Serial.printf(
        "[RFID] I2C SDA=%d SCL=%d\n",
        pins::I2C_SDA,
        pins::I2C_SCL);

    // GPIO35 est une entree uniquement; le module PN532 doit fournir son
    // pull-up IRQ. GPIO26 pilote RSTPD_N avant l'initialisation du lecteur.
    configurePins();
    attachInterruptArg(
        pins::PN532_IRQ,
        &RfidDriver::irqHandler,
        this,
        FALLING);
    _irqAttached = true;

    /*
     * Le PN532 utilise le bus I2C fourni par I2cHal. IRQ et RESET restent
     * sur leurs GPIO dédiés; RESET est utilisé pour repartir d'un état connu.
     */
    Serial.println(
        "[RFID] PN532 I2C: IRQ=GPIO35, RESET=GPIO26");

    /*
     * Recherche du PN532
     */
    Serial.println(
        "[RFID] Recherche PN532 sur I2C...");

    if (!waitForPn532Availability(
            PN532_DETECTION_TIMEOUT_MS,
            PN532_DETECTION_POLL_MS)) {

        Serial.println(
            "[RFID][ERROR] PN532 absent sur I2C");

        _initialized = false;
        _healthy = false;
        _active = false;

        return constants::ErrorCode::RFID_FAILURE;
    }

    Serial.println(
        "[RFID] PN532 detecte a l'adresse 0x24");

    /*
     * Configuration
     */
    if (!configurePn532()) {

        Serial.println(
            "[RFID][ERROR] Configuration PN532 echouee");

        _initialized = false;
        _healthy = false;
        _active = false;

        return constants::ErrorCode::RFID_FAILURE;
    }

#else

    return constants::ErrorCode::RFID_FAILURE;

#endif

    _initialized = true;
    _healthy = true;
    _active = true;

    Serial.println(
        "[RFID] Driver RFID initialise");

    return constants::ErrorCode::OK;
}


/* ============================================================
 * POLL
 * ============================================================ */

constants::ErrorCode RfidDriver::poll()
{
    if (!_initialized) {
        return constants::ErrorCode::NOT_INITIALIZED;
    }

    if (!_healthy) {
        return constants::ErrorCode::RFID_FAILURE;
    }

    return constants::ErrorCode::OK;
}


/* ============================================================
 * START
 * ============================================================ */

bool RfidDriver::start()
{
    if (!_initialized) {
        Serial.println(
            "[RFID][ERROR] Driver non initialise");

        return false;
    }

#ifdef ESP32_TARGET

    /*
     * Le polling RFID est désormais déclenché explicitement par le service,
     * donc on n'active pas de tâche permanente qui relancerait le PN532
     * en boucle sans badge.
     */
    _active = true;
    _taskRunning = false;
    _taskStarted = false;
    _taskHandle = nullptr;

    Serial.println(
        "[RFID] Polling RFID active sur demande explicite");

    return true;

#else

    _active = true;
    return true;

#endif
}


/* ============================================================
 * STOP
 * ============================================================ */

bool RfidDriver::stop()
{
    _active = false;

#ifdef ESP32_TARGET

    _taskRunning = false;
    _passiveDetectionStarted = false;
    _awaitingDetectionAck = false;
    _irqPending = false;

    if (_taskHandle != nullptr) {

        /*
         * On laisse la tâche sortir proprement.
         */
        for (uint8_t attempt = 0U;
             attempt < 20U &&
             _taskHandle != nullptr;
             ++attempt) {

            vTaskDelay(
                pdMS_TO_TICKS(10U));
        }

        /*
         * Sécurité
         */
        if (_taskHandle != nullptr) {

            vTaskDelete(
                _taskHandle);

            _taskHandle = nullptr;
        }
    }

    _taskStarted = false;

#endif

    return true;
}


/* ============================================================
 * HEALTH
 * ============================================================ */

bool RfidDriver::isHealthy() const
{
#ifdef ESP32_TARGET

    return _initialized &&
           _healthy;

#else

    return _initialized &&
           _healthy;

#endif
}


/* ============================================================
 * INITIALIZED
 * ============================================================ */

bool RfidDriver::isInitialized() const
{
    return _initialized;
}


/* ============================================================
 * RESET
 * ============================================================ */

void RfidDriver::reset()
{
    _active = false;
    _healthy = false;

#ifdef ESP32_TARGET

    _taskRunning = false;
    _passiveDetectionStarted = false;
    _awaitingDetectionAck = false;
    _irqPending = false;

    performHardwareReset();

    /*
     * On ne considère pas immédiatement
     * le PN532 sain avant reconfiguration.
     */
    if (configurePn532()) {

        _healthy = true;
        _active = true;

        Serial.println(
            "[RFID] PN532 reset et reconfigure");

    } else {

        Serial.println(
            "[RFID][ERROR] Reconfiguration PN532 echouee");
    }

#endif
}


/* ============================================================
 * READ UID
 * ============================================================ */

bool RfidDriver::readUid(
    uint8_t* uid,
    uint8_t& uidLength,
    uint32_t timeoutMs)
{
    uidLength = 0U;

    if (!_initialized ||
        !_active ||
        !_healthy ||
        uid == nullptr) {

        Serial.println(
            "[RFID][DBG] readUid: rejected because driver not ready or uid buffer is null");

        return false;
    }

#ifdef ESP32_TARGET

    if (!_i2cHal.lock(
            hardware::I2C_MUTEX_TIMEOUT_MS)) {

        return false;
    }

    applyPn532I2cSettings();
    const bool result = _pn532.readPassiveTargetID(
        PN532_MIFARE_ISO14443A,
        uid,
        &uidLength,
        static_cast<uint16_t>(timeoutMs));
    _i2cHal.native().setTimeOut(hardware::I2C_TRANSACTION_MAX_MS);
    _i2cHal.unlock();

    return result && uidLength > 0U && uidLength <= 16U;

#else

    (void)timeoutMs;
    return false;

#endif
}


/* ============================================================
 * TASK ENTRY
 * ============================================================ */

#ifdef ESP32_TARGET

void RfidDriver::taskEntry(void* context)
{
    if (context == nullptr) {
        vTaskDelete(nullptr);
        return;
    }

    RfidDriver* driver =
        static_cast<RfidDriver*>(context);

    driver->runTask();
}


/* ============================================================
 * RFID TASK
 * ============================================================ */

void RfidDriver::runTask()
{
    _taskStarted = true;

    Serial.println(
        "[RFID] RfidTask demarrage");

    while (_taskRunning) {

        const uint32_t now = millis();
        if (_lastReadAttemptMs != 0U &&
            (now - _lastReadAttemptMs) < PN532_IDLE_POLL_INTERVAL_MS) {
            vTaskDelay(
                pdMS_TO_TICKS(50U));
            continue;
        }

        _lastReadAttemptMs = now;

        uint8_t uid[16]{};
        uint8_t uidLength = 0U;

        /*
         * Lecture réelle du PN532.
         */
        if (readHardwareUid(
                uid,
                uidLength)) {

            if (_resultMutex != nullptr) {

                if (xSemaphoreTake(
                        _resultMutex,
                        pdMS_TO_TICKS(10U)) == pdTRUE) {

                    /*
                     * Copie UID.
                     */
                    std::memcpy(
                        _resultUid,
                        uid,
                        sizeof(_resultUid));

                    _resultUidLength =
                        uidLength;

                    _resultAvailable = true;

                    xSemaphoreGive(
                        _resultMutex);

                    /*
                     * DEBUG UID
                     */
                    Serial.print(
                        "[RFID] Badge detecte UID: ");

                    for (uint8_t index = 0U;
                         index < uidLength;
                         ++index) {

                        if (index > 0U) {
                            Serial.print(":");
                        }

                        if (uid[index] < 0x10U) {
                            Serial.print("0");
                        }

                        Serial.print(
                            uid[index],
                            HEX);
                    }

                    Serial.println();
                }
            }
        }

        /*
         * Petite pause.
         */
        vTaskDelay(
            pdMS_TO_TICKS(100U));
    }

    _taskStarted = false;
    _taskHandle = nullptr;

    vTaskDelete(nullptr);
}


/* ============================================================
 * LECTURE HARDWARE PN532
 * ============================================================ */

bool RfidDriver::readHardwareUid(
    uint8_t* uid,
    uint8_t& uidLength)
{
    if (uid == nullptr) {
        return false;
    }

    uidLength = 0U;

    if (!_i2cHal.lock(
            hardware::I2C_MUTEX_TIMEOUT_MS)) {
        Serial.println(
            "[RFID][DBG] readHardwareUid: lock failed before readPassiveTargetID()");
        return false;
    }

    applyPn532I2cSettings();

    Serial.printf(
        "[RFID][DBG] readHardwareUid: calling readPassiveTargetID(type=0x%02X, timeout=%lu ms)\n",
        static_cast<unsigned int>(PN532_MIFARE_ISO14443A),
        static_cast<unsigned long>(PN532_COMMAND_TIMEOUT_MS));

    const bool result =
        _pn532.readPassiveTargetID(
            PN532_MIFARE_ISO14443A,
            uid,
            &uidLength,
            PN532_COMMAND_TIMEOUT_MS);

    _i2cHal.unlock();

    ++_responseReadCount;

    Serial.printf(
        "[RFID][DBG] readHardwareUid: readPassiveTargetID returned result=%s uidLength=%u\n",
        result ? "true" : "false",
        static_cast<unsigned int>(uidLength));

    if (!result ||
        uidLength == 0U ||
        uidLength > 16U) {
        _lastDetectionError = 2U;
        Serial.println(
            "[RFID][DBG] readHardwareUid: no ISO14443A card detected or UID length invalid");
        return false;
    }

    Serial.print(
        "[RFID][DBG] readHardwareUid: UID=");

    for (uint8_t index = 0U;
         index < uidLength;
         ++index) {

        if (index > 0U) {
            Serial.print(":");
        }

        if (uid[index] < 0x10U) {
            Serial.print("0");
        }

        Serial.print(
            uid[index],
            HEX);
    }

    Serial.println();

    _lastDetectionError = 0U;

    return true;
}

#endif

}  // namespace drivers
}  // namespace fortico
