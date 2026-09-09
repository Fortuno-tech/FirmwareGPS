#ifndef SIM7600_CLIENT_H
#define SIM7600_CLIENT_H

#include <Arduino.h>
#include <Client.h>
#include <HardwareSerial.h>

class SIM7600Client : public Client
{
public:
    explicit SIM7600Client(
        HardwareSerial& serial
    );

    int connect(
        const char* host,
        uint16_t port
    ) override;

    int connect(
        IPAddress ip,
        uint16_t port
    ) override;

    size_t write(
        uint8_t value
    ) override;

    size_t write(
        const uint8_t* buffer,
        size_t size
    ) override;

    int available() override;

    int read() override;

    int read(
        uint8_t* buffer,
        size_t size
    ) override;

    int peek() override;

    void flush() override;

    void stop() override;

    uint8_t connected() override;

    operator bool() override;

private:
    bool openSocket();

    String sendCommand(
        const String& command,
        uint32_t timeout = 5000
    );

    void drainInput();

    HardwareSerial& _serial;
    String _host;
    uint16_t _port;
    bool _connected;
    bool _socketOpen;

    uint8_t _rxBuffer[512];
    size_t _rxIndex;
    size_t _rxLength;
};

#endif
