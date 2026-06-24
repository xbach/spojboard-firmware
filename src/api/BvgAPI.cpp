#include "BvgAPI.h"
#include "../utils/Logger.h"
#include "../utils/HttpUtils.h"
#include "../utils/TimeUtils.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <cstring>

BvgAPI::BvgAPI() : statusCallback(nullptr) {}

void BvgAPI::setStatusCallback(APIStatusCallback callback)
{
    statusCallback = callback;
}

TransitAPI::APIResult BvgAPI::fetchDepartures(const Config& config)
{
    TransitAPI::APIResult result = {};
    result.departureCount = 0;
    result.hasError = false;
    result.stopName[0] = '\0';
    result.errorMsg[0] = '\0';

    // Validate inputs (use Berlin-specific field)
    if (strlen(config.berlinStopIds) == 0)
    {
        result.hasError = true;
        strlcpy(result.errorMsg, "Missing stop IDs", sizeof(result.errorMsg));
        return result;
    }

    logTimestamp();
    debugPrintln("BVG API: Fetching departures...");
    logMemory("bvg_api_start");

    // Temporary array to collect all departures from all stops
    // IMPORTANT: Static to avoid stack overflow (~2KB array)
    static Departure tempDepartures[MAX_TEMP_DEPARTURES];
    int tempCount = 0;

    // Parse comma-separated stop IDs (use Berlin-specific field)
    char stopIdsCopy[128];
    strlcpy(stopIdsCopy, config.berlinStopIds, sizeof(stopIdsCopy));

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

        // Rate limiting: 1-second delay between API calls
        delay(1000);

        stopId = strtok(NULL, ",");
    }

    // Sort all collected departures by ETA (ascending)
    qsort(tempDepartures, tempCount, sizeof(Departure), compareDepartures);

    // Copy to final array (up to MAX_DEPARTURES for caching)
    // Note: Filtering by minDepartureTime now happens in main.cpp (APIFetchTask) for consistency across all APIs
    result.departureCount = 0;
    for (int i = 0; i < tempCount && result.departureCount < MAX_DEPARTURES; i++)
    {
        result.departures[result.departureCount] = tempDepartures[i];
        result.departureCount++;
    }

    if (result.departureCount == 0)
    {
        result.hasError = true;
        strlcpy(result.errorMsg, "No departures", sizeof(result.errorMsg));
    }

    logTimestamp();
    char msg[64];
    snprintf(msg, sizeof(msg), "BVG API: Fetched %d departures", result.departureCount);
    debugPrintln(msg);
    logMemory("bvg_api_end");

    return result;
}

int BvgAPI::getStopCount(const Config& config)
{
    return countStopIds(config.berlinStopIds);
}

TransitAPI::StopResult BvgAPI::fetchStop(const Config& config, int index)
{
    TransitAPI::StopResult result = {};
    result.departureCount = 0;
    result.hasError = false;
    result.stopName[0] = '\0';
    result.errorMsg[0] = '\0';
    result.infoText[0] = '\0'; // BVG has no infotexts

    if (strlen(config.berlinStopIds) == 0)
    {
        result.hasError = true;
        strlcpy(result.errorMsg, "Missing stop IDs", sizeof(result.errorMsg));
        return result;
    }

    char stopId[16];
    if (!getStopIdAt(config.berlinStopIds, index, stopId, sizeof(stopId)))
    {
        result.hasError = true;
        strlcpy(result.errorMsg, "Stop index out of range", sizeof(result.errorMsg));
        return result;
    }

    bool isFirstStop = true;
    bool ok = querySingleStop(stopId, config, result.departures, result.departureCount, result.stopName,
                              isFirstStop);
    if (!ok)
    {
        result.hasError = true;
        strlcpy(result.errorMsg, "Stop fetch failed", sizeof(result.errorMsg));
    }
    return result;
}

bool BvgAPI::querySingleStop(const char* stopId,
                             const Config& config,
                             Departure* tempDepartures,
                             int& tempCount,
                             char* stopName,
                             bool& isFirstStop)
{
    // Build URL without server-side time offset
    // Format: https://v6.bvg.transport.rest/stops/{stopId}/departures?duration=120&results=12&when={unix_timestamp}
    char url[256];

    // Calculate offset time: current time + minDepartureTime (in seconds)
    // BVG API accepts Unix timestamps (seconds since epoch)
    // Add 90-second buffer: BVG API returns departures ~80s before 'when' time + HTTP latency
    // Device side filtering will happen after API call also, we request with filter to make response space effective
    time_t now = time(nullptr);
    time_t whenTime = now + (config.minDepartureTime * 60) + 80;

    snprintf(url,
             sizeof(url),
             "https://v6.bvg.transport.rest/stops/%s/departures?duration=120&results=%d&when=%ld",
             stopId,
             DEPS_PER_STOP,
             (long)whenTime);

    HTTPClient http;
    WiFiClientSecure client;
    configureSecureClient(client);
    http.setTimeout(HTTP_TIMEOUT_MS);

    logTimestamp();
    debugPrint("BVG API: Querying stop ");
    debugPrintln(stopId);
    logTimestamp();
    char debugMsg[384];
    snprintf(debugMsg,
             sizeof(debugMsg),
             "BVG API: URL: %s (now=%ld, when=%ld, offset=%d min)",
             url,
             (long)now,
             (long)whenTime,
             config.minDepartureTime);
    debugPrintln(debugMsg);

    bool success = false;
    int httpCode = 0;

    // Retry logic: 3 attempts with exponential backoff
    for (int attempt = 1; attempt <= 3; attempt++)
    {
        http.begin(client, url);
        // BVG API requires no authentication
        http.addHeader("Content-Type", "application/json");

        httpCode = http.GET();

        if (httpCode == HTTP_CODE_OK)
        {
            success = true;
            break;
        }
        else
        {
            logTimestamp();
            char errMsg[64];
            snprintf(errMsg, sizeof(errMsg), "BVG API: HTTP error %d (attempt %d/3)", httpCode, attempt);
            debugPrintln(errMsg);

            // Don't retry on 4xx client errors
            if (httpCode >= 400 && httpCode < 500)
            {
                break;
            }

            // Exponential backoff for retries
            if (attempt < 3)
            {
                int delayMs = attempt * 2000; // 2s, 4s, 6s
                if (statusCallback)
                {
                    char statusMsg[32];
                    snprintf(statusMsg, sizeof(statusMsg), "API Retry %d/3", attempt);
                    statusCallback(statusMsg);
                }

                // Show error briefly before retry
                delay(1000);
                delay(delayMs);
            }
        }
    }

    if (!success)
    {
        http.end();
        logTimestamp();
        debugPrintln("BVG API: Failed after retries");
        return false;
    }

    // Read the full body with the buffered reader (robust on TLS: 512-byte chunks,
    // de-chunks safely). The cap (JSON_BUFFER_SIZE) must exceed the real ~26-28KB
    // response or it truncates -> IncompleteInput.
    String payload = readHttpResponse(http, JSON_BUFFER_SIZE, config.debugMode);
    http.end();

    // Parse with a filter so the document keeps only the handful of fields we use;
    // BVG's payload is mostly fields we ignore (remarks, occupancy, trip position,
    // operator, ...). This shrinks the doc from ~24KB to a few KB. The [0] template
    // applies to every departures[] element.
    StaticJsonDocument<512> filter;
    filter["departures"][0]["direction"] = true;
    filter["departures"][0]["when"] = true;
    filter["departures"][0]["delay"] = true;
    filter["departures"][0]["platform"] = true;
    filter["departures"][0]["line"]["name"] = true;
    filter["departures"][0]["stop"]["name"] = true;

    DynamicJsonDocument doc(PARSE_DOC_SIZE);
    DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));

    if (error)
    {
        logTimestamp();
        debugPrint("BVG API: JSON parse error: ");
        debugPrintln(error.c_str());
        return false;
    }

    // BVG API returns: {"departures": [...]}
    JsonArray departures = doc["departures"].as<JsonArray>();

    if (departures.isNull())
    {
        logTimestamp();
        debugPrintln("BVG API: No departures array in response");
        return false;
    }

    logTimestamp();
    char countMsg[64];
    snprintf(countMsg, sizeof(countMsg), "BVG API: Found %d departures in JSON", departures.size());
    debugPrintln(countMsg);

    // Extract stop name from first departure if this is the first stop
    if (isFirstStop && departures.size() > 0)
    {
        JsonObject firstDep = departures[0];
        if (firstDep.containsKey("stop") && firstDep["stop"].containsKey("name"))
        {
            const char* name = firstDep["stop"]["name"];
            if (name)
            {
                strlcpy(stopName, name, 64);
            }
        }
        isFirstStop = false;
    }

    // Parse each departure
    int beforeParse = tempCount;
    for (JsonObject depJson : departures)
    {
        if (tempCount >= MAX_TEMP_DEPARTURES)
            break;

        parseDepartureObject(depJson, config, tempDepartures, tempCount, stopId);
    }

    logTimestamp();
    int parsedCount = tempCount - beforeParse;
    snprintf(countMsg, sizeof(countMsg), "BVG API: Parsed %d departures (total now: %d)", parsedCount, tempCount);
    debugPrintln(countMsg);

    return true;
}

void BvgAPI::parseDepartureObject(JsonObject depJson, const Config& config, Departure* tempDepartures, int& tempCount, const char* stopId)
{
    // Extract line name (from line.name)
    if (!depJson.containsKey("line") || !depJson["line"].containsKey("name"))
    {
        logTimestamp();
        debugPrintln("BVG API: Skipping departure - no line info");
        return; // Skip if no line info
    }

    const char* lineName = depJson["line"]["name"];
    if (!lineName || strlen(lineName) == 0)
    {
        logTimestamp();
        debugPrintln("BVG API: Skipping departure - empty line name");
        return;
    }

    strlcpy(tempDepartures[tempCount].line, lineName, sizeof(tempDepartures[tempCount].line));
    stripSpaces(tempDepartures[tempCount].line);
    stripBrackets(tempDepartures[tempCount].line);

    // Extract direction (destination)
    const char* direction = depJson["direction"].as<const char*>();
    if (!direction || strlen(direction) == 0)
    {
        logTimestamp();
        debugPrintln("BVG API: Skipping departure - no direction");
        return; // Skip if no destination
    }

    // Destination - apply prefix cleanup for RE/S lines
    strlcpy(tempDepartures[tempCount].destination, direction, sizeof(tempDepartures[tempCount].destination));

    // For RE and S lines, clean up destination prefixes
    if (lineName[0] == 'R' && lineName[1] == 'E' && (lineName[2] == '\0' || lineName[2] == ' '))
    {
        // RE line - clean up destination
        char* dest = tempDepartures[tempCount].destination;
        if (strncmp(dest, "S+U ", 4) == 0)
        {
            // Replace "S+U " with "U "
            memmove(dest, dest + 2, strlen(dest) - 1); // Shift by 2 positions, keep null terminator
        }
        else if (strncmp(dest, "S ", 2) == 0)
        {
            // Remove "S " prefix
            memmove(dest, dest + 2, strlen(dest) - 1); // Shift by 2 positions, keep null terminator
        }
    }
    else if (lineName[0] == 'S' &&
             (lineName[1] == '\0' || lineName[1] == ' ' || (lineName[1] >= '0' && lineName[1] <= '9')))
    {
        // S line (S, S1, S2, etc.) - clean up destination
        char* dest = tempDepartures[tempCount].destination;
        if (strncmp(dest, "S+U ", 4) == 0)
        {
            // Replace "S+U " with "U "
            memmove(dest, dest + 2, strlen(dest) - 1); // Shift by 2 positions, keep null terminator
        }
        else if (strncmp(dest, "S ", 2) == 0)
        {
            // Remove "S " prefix
            memmove(dest, dest + 2, strlen(dest) - 1); // Shift by 2 positions, keep null terminator
        }
    }

    // Shorten common words in destination (while still UTF-8)
    shortenDestination(tempDepartures[tempCount].destination);

    // Initialize platform field
    tempDepartures[tempCount].platform[0] = '\0';

    // Extract platform/track information
    const char* platform = depJson["platform"].as<const char*>();
    if (platform && strlen(platform) > 0)
    {
        // Parse platform: if contains space (e.g., "Gleis 12"), use part after space
        // Otherwise use entire string (e.g., "12", "A", "1a")
        const char* platformDisplay = platform;
        const char* spacePos = strchr(platform, ' ');
        if (spacePos != nullptr)
        {
            platformDisplay = spacePos + 1; // Skip "Gleis " prefix
        }

        // Copy to platform field, strip brackets, truncate to fit
        strlcpy(tempDepartures[tempCount].platform, platformDisplay, sizeof(tempDepartures[tempCount].platform));
        stripBrackets(tempDepartures[tempCount].platform);

        if (config.debugMode && strlen(platformDisplay) > 3)
        {
            char warnMsg[64];
            snprintf(
                warnMsg, sizeof(warnMsg), "BVG: Platform truncated '%s' -> '%.3s'", platformDisplay, platformDisplay);
            debugPrintln(warnMsg);
        }
    }

    // Parse ISO 8601 timestamp from "when" field
    const char* when = depJson["when"].as<const char*>();
    if (!when || strlen(when) == 0)
    {
        logTimestamp();
        debugPrintln("BVG API: Skipping departure - no timestamp");
        return; // Skip if no timestamp
    }

    // Parse: "2026-01-11T14:30:00+01:00"
    // Note: parseTimestamp() ignores timezone offset and uses configured timezone (CET/CEST)
    time_t depTime = parseTimestamp(when);

    if (depTime == -1)
    {
        logTimestamp();
        debugPrint("BVG API: Skipping departure - failed to parse timestamp: ");
        debugPrintln(when);
        return; // Skip if timestamp parse fails
    }

    tempDepartures[tempCount].departureTime = depTime;

    // Calculate ETA (minutes from now)
    time_t now = time(nullptr);
    int etaSeconds = tempDepartures[tempCount].departureTime - now;

    // Skip departures that are in the past (negative or zero etaSeconds)
    // Must check BEFORE division because -4/60 = 0 (integer division rounds toward zero)
    if (etaSeconds < 0)
    {
        logTimestamp();
        char debugMsg[64];
        snprintf(debugMsg, sizeof(debugMsg), "BVG API: Skipping departure - in the past: %d seconds", etaSeconds);
        debugPrintln(debugMsg);
        return;
    }

    tempDepartures[tempCount].eta = etaSeconds / 60;

    // Debug log for first few departures (only if debug mode enabled)
    if (config.debugMode && tempCount < 3)
    {
        logTimestamp();
        char debugMsg[128];
        snprintf(debugMsg,
                 sizeof(debugMsg),
                 "BVG: Line %s to %s - ETA:%d (Plt:%s, when:%s)",
                 lineName,
                 direction,
                 tempDepartures[tempCount].eta,
                 tempDepartures[tempCount].platform[0] ? tempDepartures[tempCount].platform : "-",
                 when);
        debugPrintln(debugMsg);
    }

    // Parse delay (seconds in BVG API)
    if (!depJson["delay"].isNull())
    {
        int delaySec = depJson["delay"] | 0;
        tempDepartures[tempCount].delayMinutes = delaySec / 60;
        tempDepartures[tempCount].isDelayed = (delaySec >= 60); // Only flag if ≥1 minute
    }
    else
    {
        tempDepartures[tempCount].delayMinutes = 0;
        tempDepartures[tempCount].isDelayed = false;
    }

    // BVG API doesn't provide AC info
    tempDepartures[tempCount].hasAC = false;

    // Source stop ID (for platform symbol matching)
    strlcpy(tempDepartures[tempCount].sourceStopId, stopId, sizeof(tempDepartures[0].sourceStopId));

    tempCount++;
}
