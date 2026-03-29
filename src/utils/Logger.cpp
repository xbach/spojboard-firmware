#include "Logger.h"
#include "TelnetLogger.h"
#include "../config/AppConfig.h"
#include <esp_heap_caps.h>

// Global config pointer for debug checks
static const Config* g_config = nullptr;

void initLogger(const Config* cfg)
{
    g_config = cfg;
}

void logTimestamp()
{
    char timestamp[24];
    snprintf(timestamp, sizeof(timestamp), "[%010lu] ", millis());
    Serial.print(timestamp);

    // Mirror to telnet if debug enabled AND telnet is active
    if (g_config && g_config->debugMode && TelnetLogger::getInstance().isActive())
    {
        TelnetLogger::getInstance().print(timestamp);
    }
}

void logMemory(const char* location)
{
    logTimestamp();
    Serial.printf("MEM@%s: Free=%u Min=%u Internal=%u MaxBlock=%u PSRAM=%u\n",
                  location, ESP.getFreeHeap(), ESP.getMinFreeHeap(),
                  heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                  heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
                  heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    // Mirror to telnet if debug enabled AND telnet is active
    if (g_config && g_config->debugMode && TelnetLogger::getInstance().isActive())
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "MEM@%s: Free=%u Min=%u\n", location, ESP.getFreeHeap(), ESP.getMinFreeHeap());
        TelnetLogger::getInstance().print(buf);
    }
}

void debugPrint(const char* message)
{
    // Always print to Serial
    Serial.print(message);

    // Mirror to telnet ONLY if debug mode enabled AND telnet is active
    if (g_config && g_config->debugMode && TelnetLogger::getInstance().isActive())
    {
        TelnetLogger::getInstance().print(message);
    }
}

void debugPrintln(const char* message)
{
    // Always print to Serial
    Serial.println(message);

    // Mirror to telnet ONLY if debug mode enabled AND telnet is active
    if (g_config && g_config->debugMode && TelnetLogger::getInstance().isActive())
    {
        TelnetLogger::getInstance().println(message);
    }
}

void debugPrint(int value)
{
    // Always print to Serial
    Serial.print(value);

    // Mirror to telnet ONLY if debug mode enabled AND telnet is active
    if (g_config && g_config->debugMode && TelnetLogger::getInstance().isActive())
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d", value);
        TelnetLogger::getInstance().print(buf);
    }
}

void debugPrint(unsigned int value)
{
    // Always print to Serial
    Serial.print(value);

    // Mirror to telnet ONLY if debug mode enabled AND telnet is active
    if (g_config && g_config->debugMode && TelnetLogger::getInstance().isActive())
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%u", value);
        TelnetLogger::getInstance().print(buf);
    }
}
