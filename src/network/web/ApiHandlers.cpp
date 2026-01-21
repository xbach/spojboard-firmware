#include "ApiHandlers.h"
#include "WebUtils.h"
#include <time.h>

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
    const char* apPassword)
{
    String json = "{";
    json += "\"success\":true,";
    json += "\"timestamp\":" + String(millis()) + ",";

    // Display state information
    json += "\"state\":{";
    json += "\"apModeActive\":" + String(apModeActive ? "true" : "false") + ",";
    json += "\"wifiConnected\":" + String(wifiConnected ? "true" : "false") + ",";
    json += "\"apiKeyConfigured\":" + String(apiKeyConfigured ? "true" : "false") + ",";
    json += "\"apiError\":" + String(apiError ? "true" : "false") + ",";
    json += "\"apiErrorMsg\":\"" + escapeJsonString(apiErrorMsg) + "\",";
    json += "\"demoModeActive\":" + String(demoModeActive ? "true" : "false") + ",";
    json += "\"restModeActive\":" + String(restModeActive ? "true" : "false") + ",";
    json += "\"restModeManual\":" + String(restModeManual ? "true" : "false") + ",";
    json += "\"departureCount\":" + String(departureCount) + ",";
    json += "\"stopName\":\"" + escapeJsonString(stopName) + "\",";
    json += "\"apSSID\":\"" + escapeJsonString(apSSID) + "\",";
    json += "\"apPassword\":\"" + escapeJsonString(apPassword) + "\"";
    json += "},";

    // Current time - split into components to match actual display layout
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);

    char dayStr[8];
    strftime(dayStr, sizeof(dayStr), "%a", timeinfo);  // e.g., "Mon"
    json += "\"day\":\"" + String(dayStr) + "\",";

    char dateStr[10];
    strftime(dateStr, sizeof(dateStr), "%b %d", timeinfo);  // e.g., "Feb 15"
    json += "\"date\":\"" + String(dateStr) + "\",";

    char timeStr[6];
    strftime(timeStr, sizeof(timeStr), "%H:%M", timeinfo);  // e.g., "14:35"
    json += "\"time\":\"" + String(timeStr) + "\",";

    // Weather (if enabled and available)
    if (weather && weather->temperature != 0)
    {
        json += "\"weather\":{";
        json += "\"temp\":" + String(weather->temperature) + ",";
        json += "\"code\":" + String(weather->weatherCode);
        json += "},";
    }

    // Departures array (only show up to 3 rows as configured)
    json += "\"departures\":[";
    for (int i = 0; i < rowsToShow; i++)
    {
        if (i > 0)
            json += ",";
        json += "{";
        json += "\"line\":\"" + String(departures[i].line) + "\",";
        json += "\"destination\":\"" + String(departures[i].destination) + "\",";
        json += "\"eta\":" + String(departures[i].eta) + ",";
        json += "\"platform\":\"" + String(departures[i].platform) + "\",";
        json += "\"hasAC\":" + String(departures[i].hasAC ? "true" : "false") + ",";
        json += "\"isDelayed\":" + String(departures[i].isDelayed ? "true" : "false");
        json += "}";
    }
    json += "]";

    json += "}";

    return json;
}

String buildCheckUpdateJson(const GitHubOTA::ReleaseInfo& info)
{
    String json = "{";

    if (info.hasError)
    {
        json += "\"available\":false,";
        json += "\"error\":\"" + escapeJsonString(info.errorMsg) + "\"";
    }
    else if (info.available)
    {
        json += "\"available\":true,";
        json += "\"releaseNumber\":" + String(info.releaseNumber) + ",";
        json += "\"releaseName\":\"" + escapeJsonString(info.releaseName) + "\",";
        json += "\"releaseNotes\":\"" + escapeJsonString(info.releaseNotes) + "\",";
        json += "\"fileName\":\"" + escapeJsonString(info.assetName) + "\",";
        json += "\"fileSize\":" + String(info.assetSize) + ",";
        json += "\"assetUrl\":\"" + escapeJsonString(info.assetUrl) + "\"";
    }
    else
    {
        json += "\"available\":false";
    }

    json += "}";

    return json;
}
