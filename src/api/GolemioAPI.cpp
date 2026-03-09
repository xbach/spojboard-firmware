#include "GolemioAPI.h"
#include "../utils/Logger.h"
#include "../utils/HttpUtils.h"
#include "../utils/TimeUtils.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

GolemioAPI::GolemioAPI() : statusCallback(nullptr) {}

void GolemioAPI::setStatusCallback(APIStatusCallback callback)
{
    statusCallback = callback;
}

TransitAPI::APIResult GolemioAPI::fetchDepartures(const Config& config)
{
    TransitAPI::APIResult result = {};
    result.departureCount = 0;
    result.hasError = false;
    result.stopName[0] = '\0';
    result.errorMsg[0] = '\0';

    // Validate inputs (use Prague-specific fields)
    if (strlen(config.pragueApiKey) == 0 || strlen(config.pragueStopIds) == 0)
    {
        result.hasError = true;
        strlcpy(result.errorMsg, "Missing API key or stop IDs", sizeof(result.errorMsg));
        return result;
    }

    logTimestamp();
    debugPrintln("API: Fetching departures...");
    logMemory("api_start");

    // Temporary array to collect all departures from all stops
    // IMPORTANT: Static to avoid stack overflow (~2KB array)
    static Departure tempDepartures[MAX_TEMP_DEPARTURES];
    int tempCount = 0;

    // Parse comma-separated stop IDs (use Prague-specific field)
    char stopIdsCopy[128];
    strlcpy(stopIdsCopy, config.pragueStopIds, sizeof(stopIdsCopy));

    char* stopId = strtok(stopIdsCopy, ",");
    bool firstStop = true;

    while (stopId != NULL && tempCount < MAX_TEMP_DEPARTURES)
    {
        // Trim whitespace
        while (*stopId == ' ')
            stopId++;

        if (strlen(stopId) == 0)
        {
            stopId = strtok(NULL, ",");
            continue;
        }

        querySingleStop(stopId, config, tempDepartures, tempCount, result.stopName, firstStop);

        delay(1000);

        stopId = strtok(NULL, ",");
    }

    // Sort all collected departures by ETA
    if (tempCount > 0)
    {
        qsort(tempDepartures, tempCount, sizeof(Departure), compareDepartures);

        char msg[64];
        snprintf(msg, sizeof(msg), "Collected %d departures from all stops", tempCount);
        logTimestamp();
        debugPrintln(msg);
    }

    // Copy to final array (no filtering here - main.cpp handles filtering after fetch)
    // Note: Device-side filtering is needed because:
    // 1. Network latency causes departures to age during HTTP request/response
    // 2. API issues may return stale data despite server-side filtering
    // 3. Server-side offset filtering is useful but not sufficient alone
    result.departureCount = 0;
    for (int i = 0; i < tempCount && result.departureCount < MAX_DEPARTURES; i++)
    {
        result.departures[result.departureCount] = tempDepartures[i];
        result.departureCount++;
    }

    char filterMsg[64];
    snprintf(filterMsg, sizeof(filterMsg), "Final departures after filtering: %d", result.departureCount);
    logTimestamp();
    debugPrintln(filterMsg);

    // Set error status if no departures found
    if (tempCount == 0)
    {
        result.hasError = true;
        strlcpy(result.errorMsg, "No departures", sizeof(result.errorMsg));
    }

    logMemory("api_complete");

    return result;
}

bool GolemioAPI::querySingleStop(const char* stopId,
                                 const Config& config,
                                 Departure* tempDepartures,
                                 int& tempCount,
                                 char* stopName,
                                 bool& isFirstStop)
{
    char queryMsg[96];
    snprintf(queryMsg, sizeof(queryMsg), "API: Querying stop %s", stopId);
    logTimestamp();
    debugPrintln(queryMsg);

    HTTPClient http;
    char url[512];

    // Query each stop with MAX_DEPARTURES to ensure good caching and sorting
    // minutesBefore semantics (per official Golemio docs):
    //   Positive: start from PAST (e.g., 5 = "5 minutes ago")
    //   Negative: start from FUTURE (e.g., -3 = "3 minutes from now", excludes 0-3 min)
    //   Zero: start from NOW (includes all future departures)
    // Use negative value to implement minDepartureTime filtering server-side
    // Device side filtering will happen after API call also, we request with filter to make response space effective
    int minutesBeforeParam = config.minDepartureTime > 0 ? (config.minDepartureTime * -1) : 0;

    snprintf(url,
             sizeof(url),
             "https://api.golemio.cz/v2/pid/departureboards?ids=%s&total=%d&preferredTimezone=Europe/"
             "Prague&minutesBefore=%d&minutesAfter=120",
             stopId,
             DEPS_PER_STOP,
             minutesBeforeParam);

    logTimestamp();
    debugPrint("Golemio API: minDepartureTime=");
    debugPrint(config.minDepartureTime);
    debugPrint(" → minutesBefore=");
    debugPrint(minutesBeforeParam);
    debugPrintln("");

    http.begin(url);
    http.addHeader("x-access-token", config.pragueApiKey);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(HTTP_TIMEOUT_MS);

    const int MAX_RETRIES = 3;
    int httpCode = -1;

    for (int retry = 0; retry < MAX_RETRIES; retry++)
    {
        if (retry > 0)
        {
            int delayMs = 2000 * retry; // 2s, 4s, 6s backoff

            // Update display with retry status
            char statusMsg[64];
            snprintf(statusMsg, sizeof(statusMsg), "API Retry %d/%d", retry, MAX_RETRIES);
            if (statusCallback)
            {
                statusCallback(statusMsg);
            }

            char retryMsg[64];
            snprintf(retryMsg, sizeof(retryMsg), "API: Retry %d after %dms", retry, delayMs);
            logTimestamp();
            debugPrintln(retryMsg);
            delay(delayMs);
        }

        httpCode = http.GET();

        // Success - break out of retry loop
        if (httpCode == HTTP_CODE_OK)
            break;

        // Don't retry on 4xx errors (client errors - won't fix with retry)
        if (httpCode >= 400 && httpCode < 500)
        {
            char clientErrMsg[64];
            snprintf(clientErrMsg, sizeof(clientErrMsg), "API: Client error %d - no retry", httpCode);
            logTimestamp();
            debugPrintln(clientErrMsg);
            break;
        }

        // Log retry-able errors
        if (retry < MAX_RETRIES - 1)
        {
            char retryableErrMsg[64];
            snprintf(retryableErrMsg, sizeof(retryableErrMsg), "API Error: HTTP %d - will retry", httpCode);
            logTimestamp();
            debugPrintln(retryableErrMsg);

            // Show error on display before first retry
            if (retry == 0 && statusCallback)
            {
                char errorMsg[64];
                if (httpCode == -1)
                {
                    snprintf(errorMsg, sizeof(errorMsg), "API Error: No Connection");
                }
                else
                {
                    snprintf(errorMsg, sizeof(errorMsg), "API Error: HTTP %d", httpCode);
                }
                statusCallback(errorMsg);
                delay(1000); // Show error message briefly
            }
        }
    }

    if (httpCode == HTTP_CODE_OK)
    {
        String payload = readHttpResponse(http, JSON_BUFFER_SIZE, config.debugMode);
        DynamicJsonDocument doc(JSON_BUFFER_SIZE);
        DeserializationError error = deserializeJson(doc, payload);

        if (error)
        {
            char jsonErrMsg[128];
            snprintf(jsonErrMsg, sizeof(jsonErrMsg), "JSON Parse Error for stop %s: %s", stopId, error.c_str());
            logTimestamp();
            debugPrintln(jsonErrMsg);
            http.end();
            return false;
        }

        // Get stop name from first stop (for display)
        if (isFirstStop && doc.containsKey("stops") && doc["stops"].size() > 0)
        {
            const char* name = doc["stops"][0]["stop_name"];
            if (name)
            {
                strlcpy(stopName, name, 64);
                // Note: UTF-8 to ISO-8859-2 conversion now handled by DisplayManager
            }
            isFirstStop = false;
        }

        // Parse departures from this stop
        if (doc.containsKey("departures"))
        {
            JsonArray deps = doc["departures"];

            for (JsonObject dep : deps)
            {
                if (tempCount >= MAX_TEMP_DEPARTURES)
                    break;

                parseDepartureObject(dep, config, tempDepartures, tempCount, stopId);
            }
        }

        http.end();
        return true;
    }
    else
    {
        char failMsg[128];
        snprintf(failMsg,
                 sizeof(failMsg),
                 "API: Failed after %d attempts for stop %s - HTTP %d",
                 MAX_RETRIES,
                 stopId,
                 httpCode);
        logTimestamp();
        debugPrintln(failMsg);
        http.end();
        return false;
    }
}

void GolemioAPI::parseDepartureObject(JsonObject depJson,
                                      const Config& config,
                                      Departure* tempDepartures,
                                      int& tempCount,
                                      const char* stopId)
{
    // Route/Line info
    const char* line = depJson["route"]["short_name"];
    if (line)
    {
        strlcpy(tempDepartures[tempCount].line, line, sizeof(tempDepartures[0].line));
        stripSpaces(tempDepartures[tempCount].line);
        stripBrackets(tempDepartures[tempCount].line);
    }
    else
    {
        tempDepartures[tempCount].line[0] = '\0';
    }

    // Destination/Headsign
    const char* headsign = depJson["trip"]["headsign"];
    if (headsign)
    {
        strlcpy(tempDepartures[tempCount].destination, headsign, sizeof(tempDepartures[0].destination));
        shortenDestination(tempDepartures[tempCount].destination); // Shorten while still UTF-8
        // Note: UTF-8 to ISO-8859-2 conversion now handled by DisplayManager
    }
    else
    {
        tempDepartures[tempCount].destination[0] = '\0';
    }

    // Parse and store departure timestamp
    const char* timestamp = depJson["departure_timestamp"]["predicted"];
    if (!timestamp)
    {
        timestamp = depJson["departure_timestamp"]["scheduled"];
    }

    if (timestamp)
    {
        time_t depTime = parseTimestamp(timestamp);

        if (depTime == -1)
        {
            logTimestamp();
            debugPrint("Golemio API: Skipping departure - failed to parse timestamp: ");
            debugPrintln(timestamp);
            return; // Skip if timestamp parse fails
        }

        // Store timestamp for future recalculation
        tempDepartures[tempCount].departureTime = depTime;

        // Calculate initial ETA for sorting/filtering
        time_t now;
        time(&now);
        int diffSec = difftime(depTime, now);
        int eta = (diffSec > 0) ? (diffSec / 60) : 0;
        tempDepartures[tempCount].eta = eta;
    }
    else
    {
        tempDepartures[tempCount].departureTime = 0;
        tempDepartures[tempCount].eta = 0;
    }

    // Air conditioning
    tempDepartures[tempCount].hasAC = depJson["trip"]["is_air_conditioned"] | false;

    // Delay info
    if (depJson.containsKey("delay") && !depJson["delay"].isNull())
    {
        tempDepartures[tempCount].isDelayed = true;
        tempDepartures[tempCount].delayMinutes = depJson["delay"]["minutes"] | 0;
    }
    else
    {
        tempDepartures[tempCount].isDelayed = false;
        tempDepartures[tempCount].delayMinutes = 0;
    }

    // Platform/track info (optional, from stop object)
    tempDepartures[tempCount].platform[0] = '\0'; // Initialize empty

    // Extract platform_code from stop object (nested in departure)
    if (depJson.containsKey("stop") && depJson["stop"].containsKey("platform_code"))
    {
        const char* platformCode = depJson["stop"]["platform_code"];
        if (platformCode && strlen(platformCode) > 0)
        {
            // Truncate to 3 characters if longer
            strlcpy(tempDepartures[tempCount].platform, platformCode, 4);

            if (config.debugMode && strlen(platformCode) > 3)
            {
                char warnMsg[64];
                snprintf(
                    warnMsg, sizeof(warnMsg), "Golemio: Platform truncated '%s' -> '%.3s'", platformCode, platformCode);
                debugPrintln(warnMsg);
            }
        }
    }

    // Source stop ID (for platform symbol matching)
    strlcpy(tempDepartures[tempCount].sourceStopId, stopId, sizeof(tempDepartures[0].sourceStopId));

    // Debug log for first few departures (only if debug mode enabled)
    if (config.debugMode && tempCount < 3)
    {
        logTimestamp();
        char debugMsg[128];
        snprintf(debugMsg,
                 sizeof(debugMsg),
                 "Golemio: Line %s to %s - ETA:%d (Plt:%s, AC:%d, Delay:%d)",
                 tempDepartures[tempCount].line,
                 tempDepartures[tempCount].destination,
                 tempDepartures[tempCount].eta,
                 tempDepartures[tempCount].platform[0] ? tempDepartures[tempCount].platform : "-",
                 tempDepartures[tempCount].hasAC ? 1 : 0,
                 tempDepartures[tempCount].delayMinutes);
        debugPrintln(debugMsg);
    }

    tempCount++;
}
