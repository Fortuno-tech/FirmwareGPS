#ifndef DS3231_DRIVER_H
#define DS3231_DRIVER_H

#include <Arduino.h>

struct RTCDateTime
{
    uint16_t year;

    uint8_t month;
    uint8_t day;

    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

class DS3231Driver
{
public:

    DS3231Driver();

    bool begin();

    bool isReady() const;

    bool readDateTime(
        RTCDateTime& dateTime
    );

    bool setDateTime(
        const RTCDateTime& dateTime
    );
    
private:

    bool _ready;

    uint8_t bcdToDecimal(
        uint8_t value
    );

    uint8_t decimalToBCD(
        uint8_t value
    );
};

#endif