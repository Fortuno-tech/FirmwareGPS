#ifndef TIME_SERVICE_H
#define TIME_SERVICE_H

#include <Arduino.h>

#include "ds3231_driver.h"

class TimeService
{
public:

    TimeService();

    bool begin();

    bool getDateTime(
        RTCDateTime& dateTime
    );
    
    bool setDateTime(
    const RTCDateTime& dateTime
);

    String getISO8601();

    bool isReady() const;

private:

    DS3231Driver _rtc;

    bool _ready;
};

#endif