#include "sim7600_client.h"

#include "../../include/config/hardware_config.h"

SIM7600Client::SIM7600Client(
    HardwareSerial& serial
)
    : _serial(serial),
      _host(""),
      _port(0),
      _connected(false),
      _socketOpen(false),
      _rxIndex(0),
      _rxLength(0)
{
    memset(_rxBuffer, 0, sizeof(_rxBuffer));
}

bool SIM7600Client::openSocket()
{
    if (_socketOpen)
    {
        return true;
    }

    String command = "AT+CIPSTART=\"TCP\",\"" + _host + "\"," + String(_port);
    String response = sendCommand(command, 20000);

    if (
        response.indexOf("OK") >= 0 ||
        response.indexOf("CONNECT OK") >= 0 ||
        response.indexOf("ALREADY CONNECT") >= 0
    )
    {
        _socketOpen = true;
        _connected = true;
        return true;
    }

    _connected = false;
    return false;
}

String SIM7600Client::sendCommand(
    const String& command,
    uint32_t timeout
)
{
    while (_serial.available())
    {
        _serial.read();
    }

    _serial.println(command);

    String response = "";
    uint32_t start = millis();

    while (millis() - start < timeout)
    {
        while (_serial.available())
        {
            char c = _serial.read();
            response += c;

            if (
                response.indexOf("OK") >= 0 ||
                response.indexOf("ERROR") >= 0 ||
                response.indexOf("CONNECT OK") >= 0 ||
                response.indexOf("CLOSED") >= 0
            )
            {
                return response;
            }
        }

        delay(10);
    }

    return response;
}

void SIM7600Client::drainInput()
{
    while (_serial.available())
    {
        _serial.read();
    }
}

int SIM7600Client::connect(
    const char* host,
    uint16_t port
)
{
    _host = String(host);
    _port = port;

    if (openSocket())
    {
        return 1;
    }

    return 0;
}

int SIM7600Client::connect(
    IPAddress ip,
    uint16_t port
)
{
    _host = ip.toString();
    _port = port;

    return openSocket() ? 1 : 0;
}

size_t SIM7600Client::write(
    uint8_t value
)
{
    if (!_socketOpen)
    {
        return 0;
    }

    _serial.write(value);
    return 1;
}

size_t SIM7600Client::write(
    const uint8_t* buffer,
    size_t size
)
{
    if (!_socketOpen)
    {
        return 0;
    }

    size_t sent = _serial.write(buffer, size);
    return sent;
}

int SIM7600Client::available()
{
    return 0;
}

int SIM7600Client::read()
{
    uint8_t byte = 0;
    return read(&byte, 1) == 1 ? byte : -1;
}

int SIM7600Client::read(
    uint8_t* buffer,
    size_t size
)
{
    if (!_socketOpen)
    {
        return -1;
    }

    size_t readCount = 0;
    while (readCount < size && _serial.available())
    {
        buffer[readCount++] = static_cast<uint8_t>(_serial.read());
    }

    return readCount > 0 ? static_cast<int>(readCount) : -1;
}

int SIM7600Client::peek()
{
    if (!_serial.available())
    {
        return -1;
    }

    return _serial.peek();
}

void SIM7600Client::flush()
{
    drainInput();
}

void SIM7600Client::stop()
{
    if (_socketOpen)
    {
        String response = sendCommand("AT+CIPCLOSE", 10000);
        (void)response;
    }

    _socketOpen = false;
    _connected = false;
    _host = "";
    _port = 0;
}

uint8_t SIM7600Client::connected()
{
    return _connected ? 1 : 0;
}

SIM7600Client::operator bool()
{
    return _connected;
}
