#ifndef DASHBOARD_PAGE_H
#define DASHBOARD_PAGE_H

#include <Arduino.h>
#include "../../config/AppConfig.h"

// Build the main dashboard HTML page
// Returns a String containing the complete HTML
String buildDashboardPage(
    const Config* config,
    bool apModeActive,
    bool wifiConnected,
    const char* apSSID,
    int apClientCount,
    bool apiError,
    const char* apiErrorMsg,
    int departureCount,
    const char* stopName
);

#endif // DASHBOARD_PAGE_H
