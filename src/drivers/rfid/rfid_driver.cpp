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
        -1,
        -1,
        &i2cHal.native())
    , _taskHandle(nullptr)
    , _resultMutex(nullptr)
    , _taskRunning(false)
    , _taskStarted(false)
    , _passiveDetectionStarted(false)
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

    const bool retriesConfigured =
        _pn532.setPassiveActivationRetries(
            PN532_PASSIVE_ACTIVATION_RETRIES);

    if (!retriesConfigured) {

        _i2cHal.unlock();

        Serial.println(
            "[RFID][ERROR] Activation retries echouee");

        return false;
    }

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
    if (transmissionError != 0U) {
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
    uint8_t header[7U]{};

    const size_t headerRequestSize = wire.requestFrom(
        static_cast<int>(RFID_PN532_I2C_ADDRESS),
        static_cast<int>(sizeof(header)));

    if (headerRequestSize != sizeof(header)) {
        Serial.printf(
            "[RFID][DBG] readPassiveTargetResponse: header request failed, requested=%zu received=%zu\n",
            sizeof(header),
            headerRequestSize);
        return false;
    }

    for (uint8_t index = 0U; index < sizeof(header); ++index) {
        header[index] = static_cast<uint8_t>(wire.read());
    }

    Serial.printf(
        "[RFID][DBG] readPassiveTargetResponse: header=[%02X %02X %02X %02X %02X %02X %02X]\n",
        header[0], header[1], header[2], header[3], header[4], header[5], header[6]);

    // header[0] is the I2C status byte; the frame starts at header[1].
    if (header[0] != 0x01U ||
        header[1] != 0x00U ||
        header[2] != 0x00U ||
        header[3] != 0xFFU) {
        return false;
    }

    const uint8_t payloadLength = header[4];
    if (payloadLength < 2U ||
        header[5] != static_cast<uint8_t>(~payloadLength + 1U)) {
        return false;
    }

    const uint8_t remainingLength = payloadLength + 1U;
    uint8_t remaining[33U]{};

    if (remainingLength + 1U > sizeof(remaining)) {
        Serial.printf(
            "[RFID][DBG] readPassiveTargetResponse: remaining buffer too small, remainingLength=%u\n",
            static_cast<unsigned int>(remainingLength));
        return false;
    }

    const size_t remainingRequestSize = wire.requestFrom(
        static_cast<int>(RFID_PN532_I2C_ADDRESS),
        static_cast<int>(remainingLength + 1U));

    if (remainingRequestSize != static_cast<size_t>(remainingLength + 1U)) {
        Serial.printf(
            "[RFID][DBG] readPassiveTargetResponse: remaining request failed, requested=%u received=%zu\n",
            static_cast<unsigned int>(remainingLength + 1U),
            remainingRequestSize);
        return false;
    }

    // Chaque lecture I2C PN532 commence par un octet d'etat.
    (void)wire.read();

    for (uint8_t index = 0U; index < remainingLength; ++index) {
        remaining[index] = static_cast<uint8_t>(wire.read());
    }

    Serial.printf(
        "[RFID][DBG] readPassiveTargetResponse: remaining bytes=[%02X %02X %02X %02X %02X %02X %02X %02X ...]\n",
        remaining[0], remaining[1], remaining[2], remaining[3], remaining[4], remaining[5], remaining[6], remaining[7]);

    // A response must be sent by the PN532 and answer InListPassiveTarget.
    if (header[6] != 0xD5U ||
        remaining[0] != 0x4BU) {
        return false;
    }

    // No target is a valid response, not an I2C failure.
    if (remaining[1] == 0U) {
        return true;
    }

    if (remaining[1] != 1U ||
        remaining[6] == 0U ||
        remaining[6] > 16U ||
        static_cast<uint16_t>(7U + remaining[6] + 2U) > remainingLength ||
        uid == nullptr) {
        return false;
    }

    uidLength = remaining[6];
    for (uint8_t index = 0U; index < uidLength; ++index) {
        uid[index] = remaining[7U + index];
    }

    Serial.printf(
        "[RFID] Reponse PN532: cible=%u uidLength=%u\n",
        static_cast<unsigned int>(remaining[1]),
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

    /*
     * Mode I2C pur recommandé pour le PN532.
     * Le module répond sur SDA/SCL sans IRQ ni RESET logiciel,
     * donc on évite ici tout reset matériel forcé.
     */
    Serial.println(
        "[RFID] PN532 en mode I2C pur (IRQ/RESET non requis)");

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

    const uint32_t effectiveTimeoutMs =
        timeoutMs == 0U ? PN532_COMMAND_TIMEOUT_MS : timeoutMs;

    if (!_i2cHal.lock(
            hardware::I2C_MUTEX_TIMEOUT_MS)) {

        return false;
    }

    applyPn532I2cSettings();
    _i2cHal.native().setTimeOut(PN532_I2C_POLLING_TIMEOUT_MS);

    const bool result =
        _pn532.readPassiveTargetID(
            PN532_MIFARE_ISO14443A,
            uid,
            &uidLength,
            effectiveTimeoutMs);

    _i2cHal.native().setTimeOut(hardware::I2C_TRANSACTION_MAX_MS);
    _i2cHal.unlock();

    if (result && uidLength > 0U && uidLength <= 16U) {
        Serial.print(
            "[RFID][DBG] readUid: UID=");

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
    } else {
        Serial.println(
            "[RFID][DBG] readUid: no ISO14443A card detected or returned invalid UID length");
    }

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