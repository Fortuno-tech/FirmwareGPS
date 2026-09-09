#ifndef SIM7600_SERVICE_H
#define SIM7600_SERVICE_H

#include "../../drivers/sim7600/sim7600_driver.h"

class SIM7600Service
{
public:

    SIM7600Service();

    bool begin();

    bool runDiagnostics();

    bool waitForNetwork();

    bool connectInternet();

    bool isInternetReady();

    String getIPAddress();

    SIM7600Driver& getDriver();

private:

    SIM7600Driver _driver;

    bool _internetReady;

    String _ipAddress;
};

#endif