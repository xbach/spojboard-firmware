#include "TelnetLogger.h"
#include "Logger.h"
#include <Arduino.h>

TelnetLogger::TelnetLogger() : active(false) {}

TelnetLogger& TelnetLogger::getInstance()
{
    static TelnetLogger instance;
    return instance;
}

bool TelnetLogger::begin(uint16_t port)
{
    if (active)
    {
        return true; // Already running
    }

    // Set up connection callbacks
    telnet.onConnect([](String ip) {
        Serial.print("Telnet: Client connected from ");
        Serial.println(ip);
    });

    telnet.onDisconnect([](String ip) {
        Serial.print("Telnet: Client disconnected from ");
        Serial.println(ip);
    });

    // Start telnet server
    if (telnet.begin(port))
    {
        active = true;
        logTimestamp();
        Serial.print("Telnet server started on port ");
        Serial.println(port);
        return true;
    }
    else
    {
        logTimestamp();
        Serial.println("Telnet: Failed to start server");
        return false;
    }
}

void TelnetLogger::loop()
{
    if (active)
    {
        telnet.loop();
    }
}

void TelnetLogger::print(const char* message)
{
    if (active && hasClients())
    {
        telnet.print(message);
    }
}

void TelnetLogger::println(const char* message)
{
    if (active && hasClients())
    {
        telnet.println(message);
    }
}

bool TelnetLogger::isActive()
{
    return active;
}

bool TelnetLogger::hasClients()
{
    return active && telnet.isConnected();
}

void TelnetLogger::end()
{
    if (active)
    {
        telnet.stop();
        active = false;
        logTimestamp();
        Serial.println("Telnet server stopped");
    }
}
