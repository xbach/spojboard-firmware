#ifndef API_HANDLERS_H
#define API_HANDLERS_H

#include <Arduino.h>
#include "../../api/DepartureData.h"
#include "../../api/WeatherAPI.h"
#include "../../config/AppConfig.h"
#include "../GitHubOTA.h"

/**
 * Build JSON response for display state API
 * @param departures Array of current departures
 * @param rowsToShow Number of departures to include in JSON
 * @param weather Weather data (or nullptr)
 * @param apModeActive AP mode status
 * @param wifiConnected WiFi connection status
 * @param apiKeyConfigured API configuration status
 * @param apiError API error status
 * @param apiErrorMsg API error message
 * @param demoModeActive Demo mode status
 * @param restModeActive Rest mode status
 * @param restModeManual Manual rest mode flag
 * @param departureCount Total departure count
 * @param stopName Current stop name
 * @param apSSID AP SSID
 * @param apPassword AP password
 * @return JSON string
 */
String buildDisplayStateJson(
    const Departure* departures,
    int rowsToShow,
    const WeatherData* weather,
    bool apModeActive,
    bool wifiConnected,
    bool apiKeyConfigured,
    bool apiError,
    const char* apiErrorMsg,
    bool demoModeActive,
    bool restModeActive,
    bool restModeManual,
    int departureCount,
    const char* stopName,
    const char* apSSID,
    const char* apPassword
);

/**
 * Build JSON response for GitHub update check
 * @param info Release info from GitHubOTA
 * @return JSON string
 */
String buildCheckUpdateJson(const GitHubOTA::ReleaseInfo& info);

#endif // API_HANDLERS_H
