#include "pn532_driver.h"

#include "pin_config.h"
#include "i2c_hal.h"

namespace ForticoDrivers {

PN532Driver::PN532Driver()
    : _pn532(
        PN532_IRQ_PIN,
        PN532_RESET_PIN,
        &ForticoHAL::I2CHAL::getWire()
      ),
      _ready(false)
{
}

bool PN532Driver::begin()
{
    Serial.println();
    Serial.println("=================================");
    Serial.println("       PN532 DRIVER");
    Serial.println("=================================");

    _pn532.begin();

    uint32_t versiondata =
        _pn532.getFirmwareVersion();

    if (!versiondata)
    {
        Serial.println(
            "ERREUR : PN532 non detecte !"
        );

        _ready = false;

        return false;
    }

    Serial.println(
        "PN532 detecte !"
    );

    Serial.print(
        "Chip PN5 : 0x"
    );

    Serial.println(
        (versiondata >> 24) & 0xFF,
        HEX
    );

    Serial.print(
        "Version firmware : "
    );

    Serial.print(
        (versiondata >> 16) & 0xFF
    );

    Serial.print(".");

    Serial.println(
        (versiondata >> 8) & 0xFF
    );

    _pn532.SAMConfig();

    _ready = true;

    Serial.println(
        "PN532 pret."
    );

    return true;
}

bool PN532Driver::isReady() const
{
    return _ready;
}

bool PN532Driver::readUID(
    uint8_t* uid,
    uint8_t* uidLength,
    uint16_t timeoutMs
)
{
    if (!_ready)
    {
        return false;
    }

    if (!ForticoHAL::I2CHAL::devicePresent(PN532_I2C_ADDRESS))
    {
        Serial.println("[PN532] Perte de communication I2C (0x24)");
        return false;
    }

    return _pn532.readPassiveTargetID(
        PN532_MIFARE_ISO14443A,
        uid,
        uidLength,
        timeoutMs
    );
}

} // namespace ForticoDrivers