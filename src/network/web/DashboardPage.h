#ifndef DASHBOARD_PAGE_H
#define DASHBOARD_PAGE_H

#include <Arduino.h>
#include <WebServer.h>
#include "../../config/AppConfig.h"

// Send the main dashboard HTML page using chunked transfer encoding.
// Each section (tabs, scripts) is sent individually to avoid building
// the entire ~50KB page in RAM at once. Peak heap usage: ~5-10KB.
void sendDashboardPage(
    WebServer* server,
    const Config* config,
    bool apModeActive,
    bool wifiConnected,
    const char* apSSID,
    int apClientCount,
    bool apiError,
    const char* apiErrorMsg,
    int departureCount,
    const char* stopName,
    bool demoModeActive,
    bool restModeActive,
    bool restModeManual
);

#endif // DASHBOARD_PAGE_H
