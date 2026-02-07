#ifndef SYSTEM_TAB_H
#define SYSTEM_TAB_H

#include "../../../config/AppConfig.h"
#include <WString.h>

/**
 * Build the System tab HTML content
 * @param config Pointer to configuration structure
 * @param apModeActive Whether device is in AP mode
 * @param freeHeap Free heap memory in bytes
 * @param stopName Current stop name (if available)
 * @param departureCount Number of cached departures
 * @return HTML string for system tab
 */
String buildSystemTab(const Config* config, bool apModeActive, size_t freeHeap, const char* stopName,
                      int departureCount);

#endif // SYSTEM_TAB_H
