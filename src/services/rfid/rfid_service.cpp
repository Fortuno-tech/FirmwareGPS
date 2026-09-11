#include "services/rfid/rfid_service.h"

#include <Arduino.h>
#include <cstdio>
#include <cstring>

#include "config/hardware_config.h"
#include "constants/events.h"
#include "utils/timer_utils.h"

namespace fortico {
namespace services {

/* ============================================================
 * CONSTRUCTEUR
 * ============================================================ */

RfidService::RfidService(
    interfaces::IRfidDriver& driver,
    AuthenticationService& authentication,
    interfaces::IPublisher& publisher)

    : _driver(driver)
    , _authentication(authentication)
    , _publisher(publisher)

    , _running(false)

    , _lastUid{}
    , _lastUidLength(0U)
    , _cardPresent(false)

    , _nextPollMs(0U)

    , _updateCount(0U)
    , _uidCount(0U)
    , _readFailureCount(0U)
{
}


/* ============================================================
 * START
 * ============================================================ */

bool RfidService::start()
{
    Serial.println(
        "[RFID] Demarrage service RFID...");

    /*
     * Initialisation driver.
     */
    if (_driver.init() !=
        constants::ErrorCode::OK) {

        Serial.println(
            "[RFID][ERROR] Driver RFID init echouee");

        return false;
    }

    /*
     * Authentication service.
     */
    if (!_authentication.isRunning()) {

        if (!_authentication.start()) {

            Serial.println(
                "[RFID][ERROR] AuthenticationService start echoue");

            return false;
        }
    }

    /*
     * Start driver.
     */
    if (!_driver.start()) {

        Serial.println(
            "[RFID][ERROR] RfidDriver start echoue");

        return false;
    }

    _running = true;

    _nextPollMs =
        utils::Timer::millis();

    _updateCount = 0U;
    _uidCount = 0U;
    _readFailureCount = 0U;
    _nextPollMs = utils::Timer::millis();

    clearUid();

    Serial.println(
        "[RFID] Service RFID demarre");

    return true;
}


/* ============================================================
 * STOP
 * ============================================================ */

void RfidService::stop()
{
    _driver.stop();

    _running = false;

    clearUid();

    Serial.println(
        "[RFID] Service RFID arrete");
}


/* ============================================================
 * IS RUNNING
 * ============================================================ */

bool RfidService::isRunning() const
{
    return _running &&
           _driver.isHealthy();
}


/* ============================================================
 * UPDATE
 * ============================================================ */

void RfidService::update()
{
    if (!_running) {
        return;
    }

    const uint32_t nowMs =
        utils::Timer::millis();

    /*
     * Respect de la période RFID.
     *
     * Quand aucun badge n'est présent, on applique un cooldown pour
     * éviter de relancer la lecture PN532 en boucle à chaque tick.
     */
    if (nowMs < _nextPollMs) {
        return;
    }

    _nextPollMs = nowMs + hardware::RFID_POLL_INTERVAL_MS;

    ++_updateCount;

    /*
     * Vérification driver.
     */
    if (_driver.poll() !=
        constants::ErrorCode::OK) {

        return;
    }

    uint8_t uid[16]{};
    uint8_t uidLength = 0U;

    /*
     * Lecture UID.
     */
    const bool uidAvailable =
        _driver.readUid(
            uid,
            uidLength,
            500U);

    /*
     * ========================================================
     * AUCUN BADGE
     * ========================================================
     *
     * Ce n'est PAS une erreur.
     *
     * Si un badge était présent auparavant,
     * on considère qu'il vient d'être retiré.
     */

    if (!uidAvailable) {

        if (_cardPresent) {

            Serial.println(
                "[RFID] Badge retire");

            clearUid();
        }

        /*
         * Une lecture sans badge doit être tolérée, mais pas retriée
         * à chaque tick. On ajoute un backoff progressif pour éviter de
         * relancer requestFrom() en boucle lorsque le PN532 ne répond pas.
         */
        // Absence de badge : état normal, sans backoff ni erreur I2C.
        _readFailureCount = 0U;

        return;
    }

    _readFailureCount = 0U;

    /*
     * Vérification longueur.
     */
    if (uidLength == 0U ||
        uidLength > sizeof(uid)) {

        return;
    }

    ++_uidCount;

    /*
     * ========================================================
     * MÊME BADGE
     * ========================================================
     *
     * Le badge est toujours devant le lecteur.
     *
     * On ne renvoie pas une nouvelle authentification.
     */

    if (_cardPresent &&
        isSameUid(uid, uidLength)) {

        _readFailureCount = 0U;
        return;
    }

    /*
     * ========================================================
     * NOUVEAU BADGE
     * ========================================================
     */

    char formattedUid[
        sizeof(_lastUid) * 3U
    ]{};

    formatUid(
        uid,
        uidLength,
        formattedUid,
        sizeof(formattedUid));

    Serial.print(
        "[RFID] Nouveau badge : ");

    Serial.println(
        formattedUid);

    /*
     * Timestamp.
     */
    const uint64_t timestamp =
        utils::Timer::millis();

    /*
     * EventBus.
     */
    _publisher.publish({
        constants::EventID::RFID_CARD_READ,
        timestamp,
        0U
    });

    /*
     * Authentification.
     */
    const bool authorized =
        _authentication.authenticate(
            formattedUid,
            timestamp);

    if (authorized) {

        Serial.println(
            "[RFID] Authentification : AUTORISEE");

    } else {

        Serial.println(
            "[RFID] Authentification : REFUSEE");
    }

    /*
     * Mémorisation UID.
     */
    rememberUid(
        uid,
        uidLength);
}


/* ============================================================
 * COMPTEUR UPDATE
 * ============================================================ */

uint32_t RfidService::updateCount() const
{
    return _updateCount;
}


/* ============================================================
 * COMPTEUR UID
 * ============================================================ */

uint32_t RfidService::uidCount() const
{
    return _uidCount;
}


/* ============================================================
 * COMPTEUR ERREURS
 * ============================================================ */

uint32_t RfidService::readFailureCount() const
{
    return _readFailureCount;
}


/* ============================================================
 * COMPARER UID
 * ============================================================ */

bool RfidService::isSameUid(
    const uint8_t* uid,
    uint8_t length) const
{
    if (uid == nullptr) {
        return false;
    }

    if (length != _lastUidLength) {
        return false;
    }

    return std::memcmp(
               uid,
               _lastUid,
               length) == 0;
}


/* ============================================================
 * MEMORISER UID
 * ============================================================ */

void RfidService::rememberUid(
    const uint8_t* uid,
    uint8_t length)
{
    if (uid == nullptr ||
        length == 0U ||
        length > sizeof(_lastUid)) {

        return;
    }

    std::memset(
        _lastUid,
        0,
        sizeof(_lastUid));

    std::memcpy(
        _lastUid,
        uid,
        length);

    _lastUidLength = length;

    _cardPresent = true;
}


/* ============================================================
 * EFFACER UID
 * ============================================================ */

void RfidService::clearUid()
{
    std::memset(
        _lastUid,
        0,
        sizeof(_lastUid));

    _lastUidLength = 0U;

    _cardPresent = false;
}


/* ============================================================
 * FORMAT UID
 * ============================================================ */

void RfidService::formatUid(
    const uint8_t* uid,
    uint8_t length,
    char* output,
    uint8_t outputSize)
{
    if (uid == nullptr ||
        output == nullptr ||
        outputSize == 0U) {

        return;
    }

    output[0] = '\0';

    uint8_t offset = 0U;

    for (uint8_t index = 0U;
         index < length;
         ++index) {

        if (offset >= outputSize) {
            break;
        }

        const int written =
            std::snprintf(
                output + offset,
                outputSize - offset,
                "%s%02X",
                (index == 0U) ? "" : ":",
                uid[index]);

        if (written < 0) {

            output[0] = '\0';
            return;
        }

        if (static_cast<uint8_t>(written) >=
            (outputSize - offset)) {

            output[outputSize - 1U] =
                '\0';

            return;
        }

        offset +=
            static_cast<uint8_t>(written);
    }
}

}  // namespace services
}  // namespace fortico
