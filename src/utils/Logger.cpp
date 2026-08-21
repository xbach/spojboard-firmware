#include "Logger.h"
#include <esp_heap_caps.h>

void logTimestamp()
{
    char timestamp[24];
    snprintf(timestamp, sizeof(timestamp), "[%010lu] ", millis());
    Serial.print(timestamp);
}

void logMemory(const char* location)
{
    logTimestamp();
    Serial.printf("MEM@%s: Free=%u Min=%u Internal=%u MaxBlock=%u PSRAM=%u\n",
                  location, ESP.getFreeHeap(), ESP.getMinFreeHeap(),
                  heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                  heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),
                  heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

void debugPrint(const char* message)
{
    Serial.print(message);
}

void debugPrintln(const char* message)
{
    Serial.println(message);
}

void debugPrint(int value)
{
    Serial.print(value);
}

void debugPrint(unsigned int value)
{
    Serial.print(value);
}
