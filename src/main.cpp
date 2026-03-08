#include <Arduino.h>

// Project modules
#include "utils/Logger.h"
#include "utils/TimeUtils.h"
#include "utils/gfxlatin2.h"
#include "utils/RestMode.h"
#include "utils/TelnetLogger.h"
#include "config/AppConfig.h"
#include "api/DepartureData.h"
#include "api/GolemioAPI.h"
#include "api/BvgAPI.h"
#include "api/MqttAPI.h"
#include "api/WeatherAPI.h"
#include "api/TickerAPI.h"
#include "display/DisplayManager.h"
#include "display/DisplayController.h"
#include "network/WiFiManager.h"
#include "network/CaptivePortal.h"
#include "network/ConfigWebServer.h"

// Hardware configuration and defaults are now in config/AppConfig.h

// ============================================================================
// Global Objects
// ============================================================================
DisplayManager displayManager;
DisplayController displayController(displayManager);
WiFiManager wifiManager;
CaptivePortal captivePortal;
ConfigWebServer webServer;
GolemioAPI golemioAPI; // Prague transit API
BvgAPI bvgAPI; // Berlin transit API
MqttAPI mqttAPI; // MQTT transit API
TransitAPI* transitAPI = nullptr; // Pointer to active API (selected at runtime)
WeatherAPI weatherAPI; // Weather forecast API
TickerAPI tickerAPI;   // Twelve Data ticker API

// ============================================================================
// Configuration Storage (structure defined in config/AppConfig.h)
// ============================================================================
Config config;

// ============================================================================
// Forward Declarations
// ============================================================================
void signalDisplayUpdate(); // Thread-safe display update signaling

// ============================================================================
// Dual-Core Display Task Infrastructure
// ============================================================================
TaskHandle_t displayTaskHandle = NULL;
SemaphoreHandle_t displayMutex = NULL;

// Thread-safe display update request structure
struct DisplayUpdateRequest {
    Departure departures[MAX_DEPARTURES];
    int departureCount;
    int numDepartures;
    bool wifiConnected;
    bool apMode;
    char apSSID[64];
    char apPassword[64];
    bool apiError;
    char apiErrorMsg[64];
    char stopName[64];
    bool cityConfigured;
    bool demoMode;
    bool restMode;
    bool departuresLoading;
    WeatherData weather;
    bool tickerMode;
    TickerData ticker;
    bool needsUpdate;
} displayRequest = {.needsUpdate = false};

// ============================================================================
// API Fetch Task Infrastructure (runs on Core 1, non-blocking)
// ============================================================================
TaskHandle_t apiFetchTaskHandle = NULL;
SemaphoreHandle_t apiDataMutex = NULL;

// API fetch request flags (signals from loop to API task)
struct APIFetchRequest {
    bool fetchDeparturesNow;
    bool fetchWeatherNow;
    bool fetchTickerNow;
    unsigned long lastDeparturesFetch;
    unsigned long lastWeatherFetch;
    unsigned long lastTickerFetch;
    bool timezoneInitialized; // Set to true after initTimeSync() - prevents fetches with wrong timezone
} apiFetchRequest = {.fetchDeparturesNow = false, .fetchWeatherNow = false, .fetchTickerNow = false, .lastDeparturesFetch = 0, .lastWeatherFetch = 0, .lastTickerFetch = 0, .timezoneInitialized = false};

// ============================================================================
// Departure Data (structure defined in api/DepartureData.h)
// ============================================================================
Departure departures[MAX_DEPARTURES];
int departureCount = 0;

// ============================================================================
// State Variables
// ============================================================================
unsigned long lastDisplayUpdate = 0;
unsigned long lastEtaRecalc = 0; // For 10-second ETA recalculation
bool needsDisplayUpdate = false;
bool apiError = false;
char apiErrorMsg[64] = "";
char stopName[64] = "";
bool demoModeActive = false; // Demo mode flag - stops API polling and display updates
bool restModeActive = false; // Rest mode flag - pauses API polling and turns off display
bool restModeManual = false; // True if rest mode was activated via REST API (skip periodic check)
bool awaitingDepartures = true; // True until first API fetch completes (shows "Loading" instead of "No Departures")
int lastRestCheckMinute = -1; // Last minute when rest check triggered (0-59)
WeatherData weatherData = {}; // Global weather state
bool tickerModeActive = false;  // Ticker mode flag (candlestick chart)
TickerData tickerData = {};     // Global ticker state (protected by apiDataMutex)

// Network layer is now in network/ modules:
// - WiFiManager: WiFi connection and AP mode
// - CaptivePortal: DNS server and detection endpoints
// - ConfigWebServer: Web interface handlers

// ============================================================================
// API Status Callback - Updates display during retries
// ============================================================================
void onAPIStatus(const char* message)
{
    displayManager.drawStatus(message, "", COLOR_YELLOW);
}

// ============================================================================
// Second ETA Matching - Finds next departure for same line+destination
// ============================================================================
void attachSecondETAs(Departure* deps, int count)
{
    for (int i = 0; i < count; i++)
    {
        deps[i].secondEta = -1;
        deps[i].secondDepartureTime = 0;
        if (!config.showMultipleTimes) continue;
        for (int j = i + 1; j < count; j++)
        {
            if (strcmp(deps[i].line, deps[j].line) == 0 &&
                strcmp(deps[i].destination, deps[j].destination) == 0)
            {
                deps[i].secondEta = deps[j].eta;
                deps[i].secondDepartureTime = deps[j].departureTime;
                break;
            }
        }
    }
}

// ============================================================================
// ETA Recalculation - Updates ETAs from cached timestamps every 10s
// ============================================================================
void recalculateETAs()
{
    // Acquire mutex for thread-safe access to departure data
    if (!xSemaphoreTake(apiDataMutex, pdMS_TO_TICKS(100)))
    {
        logTimestamp();
        debugPrintln("ETA Recalc: Failed to acquire mutex, skipping");
        return;
    }

    // Recalculate ETAs from cached departureTime timestamps
    // Filter out stale departures (past their departure time)
    time_t now;
    time(&now);

    logTimestamp();
    debugPrint("ETA Recalc: ");
    debugPrint(departureCount);
    debugPrint(" deps -> ");

    int validCount = 0;
    for (int i = 0; i < departureCount; i++)
    {
        int diffSec = difftime(departures[i].departureTime, now);
        int eta = (diffSec > 0) ? (diffSec / 60) : 0;

        // Filter: Keep only FUTURE departures that meet minimum departure time threshold
        // (applies to all APIs including MQTT - filters during recalculation)
        // Note: diffSec > 0 allows eta=0 (1-59 seconds) when minDepartureTime=0
        if (diffSec > 0 && eta >= config.minDepartureTime)
        {
            // Copy departure if we're filtering out previous entries
            if (validCount != i)
            {
                departures[validCount] = departures[i];
            }
            departures[validCount].eta = eta;
            validCount++;
        }
    }

    debugPrint(validCount);
    debugPrint(" valid");
    if (validCount != departureCount)
    {
        debugPrint(" (filtered ");
        debugPrint(departureCount - validCount);
        debugPrint(")");
    }
    debugPrintln("");

    departureCount = validCount;

    // Resort departures by ETA after recalculation
    if (departureCount > 1)
    {
        logTimestamp();
        debugPrintln("ETA Recalc: Resorting departures by ETA");
        qsort(departures, departureCount, sizeof(Departure), compareDepartures);

        // Log final order (first 3)
        for (int i = 0; i < departureCount && i < 3; i++)
        {
            logTimestamp();
            char sortMsg[96];
            snprintf(sortMsg,
                     sizeof(sortMsg),
                     "  After sort [%d]: Line %s, ETA=%d min",
                     i,
                     departures[i].line,
                     departures[i].eta);
            debugPrintln(sortMsg);
        }
    }

    // Recompute second ETAs after re-sort (matching pairs may have changed)
    attachSecondETAs(departures, departureCount);

    // Release mutex
    xSemaphoreGive(apiDataMutex);

    // Reset scroll state since departures may have changed positions
    displayManager.resetScroll();

    logTimestamp();
    debugPrintln("ETA Recalc: Complete, display update triggered");
    signalDisplayUpdate();
}

// ============================================================================
// Helper Functions
// ============================================================================

// Check if current city has valid API configuration
bool isCityConfigured()
{
    if (!config.configured)
    {
        return false;
    }

    if (strcmp(config.city, "Berlin") == 0)
    {
        // Berlin only needs stop IDs
        return strlen(config.berlinStopIds) > 0;
    }
    else if (strcmp(config.city, "MQTT") == 0)
    {
        // MQTT needs broker, topics, and field mappings
        return strlen(config.mqttBroker) > 0 && strlen(config.mqttRequestTopic) > 0 &&
               strlen(config.mqttResponseTopic) > 0 && strlen(config.mqttFieldLine) > 0 &&
               strlen(config.mqttFieldDestination) > 0;
    }
    else
    {
        // Prague needs both API key and stop IDs
        return strlen(config.pragueApiKey) > 0 && strlen(config.pragueStopIds) > 0;
    }
}

// ============================================================================
// API Fetch functions moved to apiFetchTask (Core 1 background task)
// ============================================================================
// Callback Functions for ConfigWebServer
// ============================================================================

void onConfigSave(const Config& newConfig, bool wifiChanged, const char* tab)
{
    // Update config
    config = newConfig;
    saveConfig(config);

    // Apply brightness immediately
    displayManager.setBrightness(config.brightness);

    if (wifiChanged)
    {
        // Restart to apply new WiFi settings
        delay(1000);
        ESP.restart();
    }
    else
    {
        // Tab-aware refresh: only trigger API refetch when transit/connection settings change
        bool needsApiFetch = (strcmp(tab, "transit") == 0 || strcmp(tab, "all") == 0);
        bool needsWeatherFetch = (strcmp(tab, "optional") == 0 || strcmp(tab, "all") == 0);

        if (needsApiFetch)
        {
            apiFetchRequest.fetchDeparturesNow = true;
        }
        if (needsWeatherFetch)
        {
            apiFetchRequest.fetchWeatherNow = true;
        }

        // Always redraw display immediately so display/optional changes are visible
        signalDisplayUpdate();
    }
}

void onRefresh()
{
    // Signal API task for immediate refresh
    apiFetchRequest.fetchDeparturesNow = true;
    apiFetchRequest.fetchWeatherNow = true;
}

void onReboot()
{
    delay(500);
    ESP.restart();
}

void onDemoStart(const Departure* demoDepartures, int demoCount)
{
    // Enter demo mode: stop API polling and display updates
    demoModeActive = true;

    // Copy demo departures to global state (mutex-protected)
    if (xSemaphoreTake(apiDataMutex, pdMS_TO_TICKS(100)))
    {
        departureCount = (demoCount > MAX_DEPARTURES) ? MAX_DEPARTURES : demoCount;
        for (int i = 0; i < departureCount; i++)
        {
            departures[i] = demoDepartures[i];
        }
        xSemaphoreGive(apiDataMutex);
    }

    // Trigger display update with demo data
    signalDisplayUpdate();

    logTimestamp();
    debugPrintln("Demo mode activated - API polling stopped");
}

void onDemoStop()
{
    // Exit demo mode: restore previous state (rest mode or normal operation)
    demoModeActive = false;

    // Check if rest mode was active before demo started (manual or scheduled)
    if (restModeActive)
    {
        // Return to rest mode (was active before demo, stays active)
        displayManager.getDisplay()->clearScreen();
        displayManager.getDisplay()->flipDMABuffer();
        signalDisplayUpdate(); // Trigger display update to render rest mode state

        logTimestamp();
        debugPrintln("Demo stopped - returning to rest mode (display off)");
    }
    else if (tickerModeActive)
    {
        // Resume ticker mode (was active before demo)
        apiFetchRequest.fetchTickerNow = true;
        signalDisplayUpdate();

        logTimestamp();
        debugPrintln("Demo stopped - returning to ticker mode");
    }
    else
    {
        // Resume normal operation (rest mode was not active before demo)
        displayManager.setBrightness(config.brightness); // Restore brightness from config

        // Signal API task for immediate refresh
        apiFetchRequest.fetchDeparturesNow = true;
        apiFetchRequest.fetchWeatherNow = true;
        signalDisplayUpdate(); // Trigger display update to show current departures

        logTimestamp();
        debugPrintln("Demo mode deactivated - resuming normal operation");
    }
}

void onRestMode(bool enabled)
{
    if (enabled && !restModeActive)
    {
        // Enter rest mode (manual via REST API)
        restModeActive = true;
        restModeManual = true; // Mark as manually activated - skip periodic checks
        signalDisplayUpdate();

        logTimestamp();
        debugPrintln("RestMode: Activated via REST API - display off, API polling paused");
    }
    else if (!enabled && restModeActive)
    {
        // Exit rest mode
        restModeActive = false;
        restModeManual = false; // Clear manual flag
        displayManager.setBrightness(config.brightness); // Restore brightness from config

        if (!tickerModeActive)
        {
            awaitingDepartures = true; // Show loading state until fresh data arrives
            apiFetchRequest.fetchDeparturesNow = true;
        }
        else
        {
            apiFetchRequest.fetchTickerNow = true; // Resume ticker fetch
        }

        // Signal API task for immediate refresh
        apiFetchRequest.fetchWeatherNow = true; // Always (status bar)
        signalDisplayUpdate();

        logTimestamp();
        debugPrintln("RestMode: Deactivated via REST API - resuming normal operation");
    }
}

// ============================================================================
// Ticker Mode Callbacks
// ============================================================================

void onTickerStart()
{
    tickerModeActive = true;
    tickerData.valid = false; // Show "Loading Ticker..."
    apiFetchRequest.fetchTickerNow = true;
    signalDisplayUpdate();

    logTimestamp();
    debugPrintln("Ticker mode activated");
}

void onTickerStop()
{
    tickerModeActive = false;
    config.tickerEnabled = false;
    saveConfig(config);

    awaitingDepartures = true; // Show "Loading Departures..."
    displayManager.setBrightness(config.brightness);
    apiFetchRequest.fetchDeparturesNow = true;
    apiFetchRequest.fetchWeatherNow = true;
    signalDisplayUpdate();

    logTimestamp();
    debugPrintln("Ticker mode deactivated - resuming departures");
}

void onTickerMode(bool enabled)
{
    if (enabled)
    {
        // Check if ticker is configured
        if (config.tickerApiKey[0] == '\0')
        {
            logTimestamp();
            debugPrintln("Ticker: Cannot enable - no API key configured");
            return;
        }

        tickerModeActive = true;
        tickerData.valid = false;
        apiFetchRequest.fetchTickerNow = true;

        if (!config.tickerEnabled)
        {
            config.tickerEnabled = true;
            saveConfig(config);
        }

        signalDisplayUpdate();
        logTimestamp();
        debugPrintln("Ticker mode enabled via REST API");
    }
    else
    {
        onTickerStop();
    }
}

// ============================================================================
// API Fetch Task - Runs on Core 1 (handles blocking HTTP calls)
// ============================================================================
void apiFetchTask(void* parameter)
{
    logTimestamp();
    debugPrintln("APIFetchTask: Started on Core 1");

    for (;;)
    {
        unsigned long now = millis();
        bool shouldFetchDepartures = false;
        bool shouldFetchWeather = false;
        bool shouldFetchTicker = false;

        // Check if immediate fetch is requested (from config save, refresh button, etc.)
        if (apiFetchRequest.fetchDeparturesNow)
        {
            apiFetchRequest.fetchDeparturesNow = false;
            shouldFetchDepartures = true;
            logTimestamp();
            debugPrintln("APIFetchTask: Immediate departures fetch requested");
        }

        if (apiFetchRequest.fetchWeatherNow)
        {
            apiFetchRequest.fetchWeatherNow = false;
            shouldFetchWeather = true;
            logTimestamp();
            debugPrintln("APIFetchTask: Immediate weather fetch requested");
        }

        if (apiFetchRequest.fetchTickerNow)
        {
            apiFetchRequest.fetchTickerNow = false;
            shouldFetchTicker = true;
            logTimestamp();
            debugPrintln("APIFetchTask: Immediate ticker fetch requested");
        }

        // Check periodic fetch intervals (only if not in demo/rest mode and WiFi connected)
        // CRITICAL: Also require timezone to be initialized to prevent timestamp parsing with wrong timezone
        if (!demoModeActive && !restModeActive && wifiManager.isConnected() && apiFetchRequest.timezoneInitialized)
        {
            // Departures fetch interval (blocked during ticker mode)
            if (!tickerModeActive && isCityConfigured())
            {
                unsigned long departuresInterval = (unsigned long)config.refreshInterval * 1000;
                if (now - apiFetchRequest.lastDeparturesFetch >= departuresInterval ||
                    apiFetchRequest.lastDeparturesFetch == 0)
                {
                    shouldFetchDepartures = true;
                }
            }

            // Weather fetch interval (allowed during ticker - status bar visible)
            if (config.weatherEnabled && config.weatherLatitude != 0.0 && config.weatherLongitude != 0.0)
            {
                unsigned long weatherInterval = (unsigned long)config.weatherRefreshInterval * 60000;
                if (now - apiFetchRequest.lastWeatherFetch >= weatherInterval ||
                    apiFetchRequest.lastWeatherFetch == 0)
                {
                    shouldFetchWeather = true;
                }
            }

            // Ticker fetch interval (only when ticker active)
            if (tickerModeActive && config.tickerApiKey[0] != '\0')
            {
                unsigned long tickerInterval = (unsigned long)config.tickerRefreshInterval * 1000;
                if (now - apiFetchRequest.lastTickerFetch >= tickerInterval ||
                    apiFetchRequest.lastTickerFetch == 0)
                {
                    shouldFetchTicker = true;
                }
            }
        }

        // Fetch departures (blocking HTTP call)
        if (shouldFetchDepartures)
        {
            apiFetchRequest.lastDeparturesFetch = now;

            logTimestamp();
            debugPrintln("APIFetchTask: Fetching departures (blocking)...");
            logTimestamp();
            debugPrint("APIFetchTask: config.minDepartureTime = ");
            debugPrint(config.minDepartureTime);
            debugPrintln("");

            // Log current device time for debugging time sync issues
            time_t currentTime;
            time(&currentTime);
            struct tm timeinfo;
            if (getLocalTime(&timeinfo))
            {
                char deviceTimeStr[32];
                strftime(deviceTimeStr, sizeof(deviceTimeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
                logTimestamp();
                debugPrint("APIFetchTask: Device time = ");
                debugPrint(deviceTimeStr);
                debugPrint(" (unix=");
                debugPrint((int)currentTime);
                debugPrintln(")");
            }
            else
            {
                logTimestamp();
                debugPrintln("⚠️ APIFetchTask: Device time not synced!");
            }

            // Call API client (blocking operation)
            TransitAPI::APIResult result = transitAPI->fetchDepartures(config);

            // Filter out stale departures (already departed) and those below minDepartureTime
            // This catches API issues, network latency, and stale data from any API source
            int validCount = 0;
            time_t now;
            time(&now);

            for (int i = 0; i < result.departureCount; i++)
            {
                // Check if departure is in the future (not already departed)
                int diffSec = difftime(result.departures[i].departureTime, now);

                // Calculate ETA (0 for departures in 0-59 seconds, or already departed)
                int freshEta = (diffSec > 0) ? (diffSec / 60) : 0;

                // Keep only FUTURE departures that meet minimum time threshold
                // Note: diffSec > 0 ensures we only keep departures that haven't left yet
                // This allows eta=0 (1-59 seconds) to display as "<1'" when minDepartureTime=0
                if (diffSec > 0 && freshEta >= config.minDepartureTime)
                {
                    // Update with fresh ETA and copy to valid position
                    result.departures[validCount] = result.departures[i];
                    result.departures[validCount].eta = freshEta;
                    validCount++;
                }
            }

            // Log filtering stats
            if (validCount != result.departureCount)
            {
                logTimestamp();
                char filterMsg[80];
                snprintf(filterMsg, sizeof(filterMsg),
                         "APIFetchTask: Filtered %d stale departures (%d -> %d valid)",
                         result.departureCount - validCount, result.departureCount, validCount);
                debugPrintln(filterMsg);
            }

            result.departureCount = validCount;

            // Attach second ETAs for same line+destination pairs
            attachSecondETAs(result.departures, result.departureCount);

            // Update global state with mutex protection
            if (xSemaphoreTake(apiDataMutex, pdMS_TO_TICKS(100)))
            {
                departureCount = result.departureCount;
                for (int i = 0; i < result.departureCount; i++)
                {
                    departures[i] = result.departures[i];
                }
                strlcpy(stopName, result.stopName, sizeof(stopName));
                apiError = result.hasError;
                if (result.hasError)
                {
                    strlcpy(apiErrorMsg, result.errorMsg, sizeof(apiErrorMsg));
                }
                awaitingDepartures = false;
                xSemaphoreGive(apiDataMutex);

                // Signal display update
                signalDisplayUpdate();

                logTimestamp();
                debugPrintln("APIFetchTask: Departures fetch complete");
            }
            else
            {
                logTimestamp();
                debugPrintln("APIFetchTask: Failed to acquire mutex for departures update");
            }
        }

        // Fetch weather (blocking HTTP call)
        if (shouldFetchWeather)
        {
            apiFetchRequest.lastWeatherFetch = now;

            logTimestamp();
            debugPrintln("APIFetchTask: Fetching weather (blocking)...");

            // Call weather API (blocking operation)
            WeatherData newWeatherData = weatherAPI.fetchWeather(config.weatherLatitude, config.weatherLongitude);

            // Update global weather state with mutex protection
            if (xSemaphoreTake(apiDataMutex, pdMS_TO_TICKS(100)))
            {
                weatherData = newWeatherData;
                xSemaphoreGive(apiDataMutex);

                // Signal display update
                signalDisplayUpdate();

                if (weatherData.hasError)
                {
                    logTimestamp();
                    debugPrint("APIFetchTask: Weather error - ");
                    debugPrintln(weatherData.errorMsg);
                }
                else
                {
                    logTimestamp();
                    char msg[64];
                    snprintf(msg, sizeof(msg), "APIFetchTask: Weather updated: %d°C", weatherData.temperature);
                    debugPrintln(msg);
                }
            }
            else
            {
                logTimestamp();
                debugPrintln("APIFetchTask: Failed to acquire mutex for weather update");
            }
        }

        // Fetch ticker data (blocking HTTP call)
        if (shouldFetchTicker)
        {
            apiFetchRequest.lastTickerFetch = now;

            logTimestamp();
            debugPrintln("APIFetchTask: Fetching ticker (blocking)...");

            // Call Twelve Data API (blocking operation)
            TickerData newTickerData = tickerAPI.fetchTicker(
                config.tickerSymbol, config.tickerInterval, config.tickerApiKey);

            // Update global ticker state with mutex protection
            if (xSemaphoreTake(apiDataMutex, pdMS_TO_TICKS(100)))
            {
                // Keep last valid data on error
                if (newTickerData.valid || !tickerData.valid)
                {
                    tickerData = newTickerData;
                }
                else if (newTickerData.hasError)
                {
                    tickerData.hasError = true; // Flag error but keep old candles
                }
                xSemaphoreGive(apiDataMutex);

                // Signal display update
                signalDisplayUpdate();

                logTimestamp();
                if (newTickerData.valid)
                {
                    char msg[64];
                    snprintf(msg, sizeof(msg), "APIFetchTask: Ticker updated: %d candles", newTickerData.candleCount);
                    debugPrintln(msg);
                }
                else
                {
                    debugPrintln("APIFetchTask: Ticker fetch failed");
                }
            }
            else
            {
                logTimestamp();
                debugPrintln("APIFetchTask: Failed to acquire mutex for ticker update");
            }
        }

        // Sleep for 100ms between checks (prevents busy-waiting, allows web server to run)
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ============================================================================
// Display Render Task - Runs on Core 1
// ============================================================================
void displayRenderTask(void* parameter)
{
    logTimestamp();
    debugPrintln("DisplayTask: Started on Core 1");

    for (;;)
    {
        // Wait for display update notification (blocks until signaled)
        uint32_t notificationValue;
        if (xTaskNotifyWait(0, 0, &notificationValue, portMAX_DELAY))
        {
            // Copy display data from request structure (thread-safe)
            Departure localDepartures[MAX_DEPARTURES];
            int localDepartureCount;
            int localNumDepartures;
            bool localWifiConnected;
            bool localApMode;
            char localApSSID[64];
            char localApPassword[64];
            bool localApiError;
            char localApiErrorMsg[64];
            char localStopName[64];
            bool localCityConfigured;
            bool localDemoMode;
            bool localRestMode;
            bool localDeparturesLoading;
            WeatherData localWeather;
            bool localTickerMode;
            TickerData localTickerData;

            // Take mutex to safely copy display request
            if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)))
            {
                if (!displayRequest.needsUpdate)
                {
                    xSemaphoreGive(displayMutex);
                    continue; // Nothing to update
                }

                // Copy all data locally
                memcpy(localDepartures, displayRequest.departures,
                       sizeof(Departure) * displayRequest.departureCount);
                localDepartureCount = displayRequest.departureCount;
                localNumDepartures = displayRequest.numDepartures;
                localWifiConnected = displayRequest.wifiConnected;
                localApMode = displayRequest.apMode;
                strlcpy(localApSSID, displayRequest.apSSID, sizeof(localApSSID));
                strlcpy(localApPassword, displayRequest.apPassword, sizeof(localApPassword));
                localApiError = displayRequest.apiError;
                strlcpy(localApiErrorMsg, displayRequest.apiErrorMsg, sizeof(localApiErrorMsg));
                strlcpy(localStopName, displayRequest.stopName, sizeof(localStopName));
                localCityConfigured = displayRequest.cityConfigured;
                localDemoMode = displayRequest.demoMode;
                localRestMode = displayRequest.restMode;
                localDeparturesLoading = displayRequest.departuresLoading;
                localWeather = displayRequest.weather;
                localTickerMode = displayRequest.tickerMode;
                localTickerData = displayRequest.ticker;

                displayRequest.needsUpdate = false;
                xSemaphoreGive(displayMutex);
            }
            else
            {
                // Timeout - skip this update
                logTimestamp();
                debugPrintln("DisplayTask: Mutex timeout, skipping update");
                continue;
            }

            // Render display with local copy (no mutex needed during render)
            // Use local weather snapshot instead of global pointer (thread-safe)
            displayManager.setWeatherData(&localWeather);

            logTimestamp();
            debugPrintln("DisplayTask: Rendering on Core 1");

            displayController.render(localDepartures,
                                    localDepartureCount,
                                    localNumDepartures,
                                    localWifiConnected,
                                    localApMode,
                                    localApSSID,
                                    localApPassword,
                                    localApiError,
                                    localApiErrorMsg,
                                    localStopName,
                                    localCityConfigured,
                                    localDemoMode,
                                    localRestMode,
                                    localDeparturesLoading,
                                    localTickerMode,
                                    &localTickerData);

            logTimestamp();
            debugPrintln("DisplayTask: Render complete");
        }

        // Small yield to prevent task starvation
        taskYIELD();
    }
}

// Helper function to signal display update (thread-safe)
void signalDisplayUpdate()
{
    if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(50)))
    {
        // Copy current state to display request
        memcpy(displayRequest.departures, departures,
               sizeof(Departure) * departureCount);
        displayRequest.departureCount = departureCount;
        displayRequest.numDepartures = config.numDepartures;
        displayRequest.wifiConnected = wifiManager.isConnected();
        displayRequest.apMode = wifiManager.isAPMode();
        strlcpy(displayRequest.apSSID, wifiManager.getAPSSID(), sizeof(displayRequest.apSSID));
        strlcpy(displayRequest.apPassword, wifiManager.getAPPassword(), sizeof(displayRequest.apPassword));
        displayRequest.apiError = apiError;
        strlcpy(displayRequest.apiErrorMsg, apiErrorMsg, sizeof(displayRequest.apiErrorMsg));
        strlcpy(displayRequest.stopName, stopName, sizeof(displayRequest.stopName));
        displayRequest.cityConfigured = isCityConfigured();
        displayRequest.demoMode = demoModeActive;
        displayRequest.restMode = restModeActive;
        displayRequest.departuresLoading = awaitingDepartures;
        displayRequest.weather = weatherData;
        displayRequest.tickerMode = tickerModeActive;
        displayRequest.ticker = tickerData;
        displayRequest.needsUpdate = true;

        xSemaphoreGive(displayMutex);

        // Notify display task to wake up and render
        xTaskNotify(displayTaskHandle, 1, eSetValueWithOverwrite);
    }
    else
    {
        logTimestamp();
        debugPrintln("signalDisplayUpdate: Mutex timeout, update skipped");
    }
}

// ============================================================================
// Setup
// ============================================================================
void setup()
{
    Serial.begin(115200);
    delay(1000);

    // Boot banner always prints to Serial (before logger init)
    Serial.println("\n╔═══════════════════════════════════════╗");
    Serial.println("║          SpojBoard v" FIRMWARE_RELEASE "                 ║");
    Serial.println("║   Smart Panel for Onward Journeys     ║");
    Serial.println("╚═══════════════════════════════════════╝\n");

    // CRITICAL: Verify firmware matches hardware variant
    if (!verifyHardware())
    {
        Serial.println("\nHalting to prevent potential hardware damage.");
        Serial.println("Please flash the correct firmware for your board.\n");
        while (1)
        {
            delay(1000); // Halt forever
        }
    }

    logMemory("boot");

    // Initialize display mutex FIRST (before any display operations)
    displayMutex = xSemaphoreCreateMutex();
    if (displayMutex == NULL)
    {
        Serial.println("FATAL: Failed to create display mutex!");
        while (1)
        {
            delay(1000);
        }
    }
    Serial.println("Display mutex created");

    // Initialize API data mutex (protects departures[] and weatherData)
    apiDataMutex = xSemaphoreCreateMutex();
    if (apiDataMutex == NULL)
    {
        Serial.println("FATAL: Failed to create API data mutex!");
        while (1)
        {
            delay(1000);
        }
    }
    Serial.println("API data mutex created");

    // Load configuration FIRST (needed for display brightness)
    loadConfig(config);

    // Initialize logger with config for debug mode checks (MUST be after loadConfig)
    initLogger(&config);

    // Select transit API based on city configuration
    if (strcmp(config.city, "Berlin") == 0)
    {
        transitAPI = &bvgAPI;
        Serial.println("Using Berlin BVG API");
    }
    else if (strcmp(config.city, "MQTT") == 0)
    {
        transitAPI = &mqttAPI;
        Serial.println("Using MQTT API");
    }
    else
    {
        transitAPI = &golemioAPI;
        Serial.println("Using Prague Golemio API");
    }

    // Set up API status callback for display updates
    transitAPI->setStatusCallback(onAPIStatus);

    // Initialize display with correct brightness from config
    if (!displayManager.begin(config.brightness))
    {
        debugPrintln("Display initialization failed!");
        return;
    }
    displayManager.setConfig(&config);
    logMemory("display_init");

    displayManager.drawStatus("Starting SpojBoard...", "FW v" FIRMWARE_RELEASE, COLOR_WHITE);

    // Create display render task on Core 1 (separate from WiFi/network on Core 0)
    BaseType_t taskCreated = xTaskCreatePinnedToCore(
        displayRenderTask,      // Task function
        "DisplayRender",        // Task name (for debugging)
        10240,                  // Stack size (10KB - display operations + platform symbol matching)
        NULL,                   // Task parameters
        2,                      // Priority (2 = above idle, below critical network tasks)
        &displayTaskHandle,     // Task handle
        1                       // Core 1 (isolated from WiFi/web server on Core 0)
    );

    if (taskCreated != pdPASS || displayTaskHandle == NULL)
    {
        Serial.println("FATAL: Failed to create display task!");
        while (1)
        {
            delay(1000);
        }
    }

    logTimestamp();
    debugPrintln("Display task created on Core 1");
    delay(100); // Give display task time to start

    // Create API fetch task on Core 1 (handles blocking HTTP calls without blocking loop)
    taskCreated = xTaskCreatePinnedToCore(
        apiFetchTask,           // Task function
        "APIFetch",             // Task name (for debugging)
        10240,                  // Stack size (10KB - HTTP client + APIResult on stack)
        NULL,                   // Task parameters
        1,                      // Priority (1 = lower than display task, allows web server to preempt)
        &apiFetchTaskHandle,    // Task handle
        1                       // Core 1 (same as loop/web server)
    );

    if (taskCreated != pdPASS || apiFetchTaskHandle == NULL)
    {
        Serial.println("FATAL: Failed to create API fetch task!");
        while (1)
        {
            delay(1000);
        }
    }

    logTimestamp();
    debugPrintln("API fetch task created on Core 1");
    delay(100); // Give API task time to start

    // Try to connect to WiFi (will fall back to AP mode if fails)
    if (!wifiManager.connectSTA(config, 20, 500))
    {
        // Connection failed, start AP mode
        displayManager.drawStatus("WiFi Failed!", "Starting AP mode...", COLOR_RED);
        delay(1500);

        if (!wifiManager.startAP())
        {
            debugPrintln("AP Mode failed to start!");
            displayManager.drawStatus("AP Mode Failed!", "", COLOR_RED);
            return;
        }

        // Start captive portal
        if (!captivePortal.begin(wifiManager.getAPIP()))
        {
            debugPrintln("Captive portal failed to start!");
        }
    }
    else
    {
        // WiFi connected successfully
        char ipStr[32];
        sprintf(ipStr, "IP: %s", WiFi.localIP().toString().c_str());
        displayManager.drawStatus("WiFi Connected!", ipStr, COLOR_GREEN);
        delay(1500);

        // CRITICAL: Initialize timezone IMMEDIATELY after WiFi connects
        // This must happen before any API fetches to ensure mktime() uses correct timezone
        logTimestamp();
        debugPrintln("Initializing timezone configuration...");
        initTimeSync();
        apiFetchRequest.timezoneInitialized = true; // Allow API fetches now

        // Start telnet logger if debug mode enabled
        if (config.debugMode)
        {
            TelnetLogger::getInstance().begin(23);
            logTimestamp();
            debugPrintln("Debug mode enabled - telnet logging active");
        }
    }

    // Initialize web server with callbacks
    webServer.setCallbacks(onConfigSave, onRefresh, onReboot, onDemoStart, onDemoStop, onRestMode,
                           onTickerStart, onTickerStop, onTickerMode);
    webServer.setDisplayManager(&displayManager); // For OTA progress updates
    if (!webServer.begin())
    {
        debugPrintln("Web server failed to start!");
    }

    // Weather data is now passed via DisplayUpdateRequest snapshot pattern
    // (no direct pointer sharing - thread-safe copy in signalDisplayUpdate)

    // Setup captive portal detection handlers
    if (wifiManager.isAPMode())
    {
        captivePortal.setupDetectionHandlers(webServer.getServer());
    }

    // Setup NTP time if connected to WiFi
    if (wifiManager.isConnected() && !wifiManager.isAPMode())
    {
        // Note: initTimeSync() was already called immediately after WiFi connected
        // to ensure timezone is set before any API fetches occur

        bool ntpSuccess = syncTime(10, 500);

        // Verify time is actually synced (not at epoch)
        if (ntpSuccess)
        {
            if (!isTimeSynced())
            {
                logTimestamp();
                debugPrintln("⚠️ WARNING: NTP reported success but time still invalid!");
                ntpSuccess = false;
            }
        }

        if (!ntpSuccess)
        {
            logTimestamp();
            debugPrintln("⚠️ WARNING: Proceeding without time sync - ETA calculations will be incorrect!");
        }

        // Signal API task for initial fetch if configured
        if (isCityConfigured())
        {
            apiFetchRequest.fetchDeparturesNow = true;
            logTimestamp();
            debugPrintln("Setup: Signaled API task for initial departures fetch");
        }

        // Signal API task for initial weather fetch if configured
        if (config.weatherEnabled && config.weatherLatitude != 0.0 && config.weatherLongitude != 0.0)
        {
            apiFetchRequest.fetchWeatherNow = true;
            logTimestamp();
            debugPrintln("Setup: Signaled API task for initial weather fetch");
        }

        // Boot activation: restore ticker mode if persistently enabled
        if (config.tickerEnabled && config.tickerApiKey[0] != '\0')
        {
            tickerModeActive = true;
            apiFetchRequest.fetchTickerNow = true;
            logTimestamp();
            debugPrintln("Setup: Ticker mode restored from persistent config");
        }
    }

    signalDisplayUpdate();
    logTimestamp();
    debugPrintln("Setup complete!\n");
}

// ============================================================================
// Main Loop
// ============================================================================
void loop()
{
    // Handle DNS for captive portal (AP mode only)
    if (wifiManager.isAPMode())
    {
        captivePortal.processRequests();
    }

    // Handle web server requests
    webServer.handleClient();

    // Process telnet connections if debug enabled
    if (config.debugMode)
    {
        TelnetLogger::getInstance().loop();
    }

    // Update web server state for status display
    webServer.updateState(&config,
                          wifiManager.isConnected(),
                          wifiManager.isAPMode(),
                          wifiManager.getAPSSID(),
                          wifiManager.getAPPassword(),
                          wifiManager.getAPClientCount(),
                          apiError,
                          apiErrorMsg,
                          departureCount,
                          stopName,
                          demoModeActive,
                          restModeActive,
                          restModeManual,
                          tickerModeActive);

    // Skip WiFi monitoring and API calls in AP mode
    if (wifiManager.isAPMode())
    {
        // Update display periodically in AP mode
        if (millis() - lastDisplayUpdate >= 5000)
        {
            lastDisplayUpdate = millis();
            signalDisplayUpdate(); // Signal Core 1 display task to render
        }

        // Check if needsDisplayUpdate flag is still used elsewhere
        if (needsDisplayUpdate)
        {
            needsDisplayUpdate = false;
            signalDisplayUpdate(); // Signal Core 1 display task to render
        }

        delay(10);
        return;
    }

    // Check WiFi connection (STA mode only)
    bool isConnected = wifiManager.isConnected();
    static bool wasConnected = isConnected; // Initialize to current state on first call

    if (!isConnected && wasConnected)
    {
        logTimestamp();
        debugPrintln("WiFi: Disconnected!");
        signalDisplayUpdate();

        // Attempt reconnection
        static unsigned long lastReconnectAttempt = 0;
        if (millis() - lastReconnectAttempt > 30000)
        {
            lastReconnectAttempt = millis();
            wifiManager.attemptReconnect();
        }
    }
    else if (isConnected && !wasConnected)
    {
        logTimestamp();
        debugPrintln("WiFi: Reconnected!");
        signalDisplayUpdate();
    }
    wasConnected = isConnected;

    // Check rest mode at :00 and :30 minutes (twice per hour, synchronized to clock)
    // Skip periodic check if rest mode was manually activated via REST API
    if (!wifiManager.isAPMode() && !restModeManual)
    {
        struct tm timeinfo;
        if (getCurrentTime(&timeinfo))
        {
            int currentMinute = timeinfo.tm_min;

            // Trigger check at :00 and :30 minutes (avoid duplicate checks in same minute)
            if ((currentMinute == 0 || currentMinute == 30) && currentMinute != lastRestCheckMinute)
            {
                lastRestCheckMinute = currentMinute;
                bool shouldBeInRest = isInRestPeriod(config.restModePeriods);

                if (shouldBeInRest && !restModeActive)
                {
                    // Enter rest mode (scheduled)
                    restModeActive = true;
                    signalDisplayUpdate();
                    logTimestamp();
                    debugPrintln("RestMode: Activated (scheduled) - display off, API polling paused");
                }
                else if (!shouldBeInRest && restModeActive)
                {
                    // Exit rest mode (scheduled period ended)
                    restModeActive = false;
                    displayManager.setBrightness(config.brightness); // Restore brightness from config

                    if (!tickerModeActive)
                    {
                        awaitingDepartures = true; // Show loading state until fresh data arrives
                        apiFetchRequest.fetchDeparturesNow = true;
                    }
                    else
                    {
                        apiFetchRequest.fetchTickerNow = true; // Resume ticker fetch
                    }

                    // Signal API task for immediate refresh
                    apiFetchRequest.fetchWeatherNow = true; // Always (status bar)
                    signalDisplayUpdate();
                    logTimestamp();
                    debugPrintln("RestMode: Deactivated (scheduled) - resuming normal operation");
                }
            }
        }
    }

    // Skip ETA recalculation in demo mode, rest mode, or ticker mode
    // (API fetching now handled by dedicated apiFetchTask on Core 1)
    if (!demoModeActive && !restModeActive && !tickerModeActive)
    {
        // Real-time ETA recalculation every 10 seconds (only when connected and have departures)
        if (wifiManager.isConnected() && departureCount > 0)
        {
            unsigned long now = millis();
            if (now - lastEtaRecalc >= 10000 || lastEtaRecalc == 0)
            {
                lastEtaRecalc = now;
                recalculateETAs();
            }
        }
    }

    // Update display (rest mode handled by DisplayController)
    // Display rendering now happens on Core 1 via displayRenderTask
    if (needsDisplayUpdate)
    {
        needsDisplayUpdate = false;
        signalDisplayUpdate(); // Signal Core 1 display task to render
    }

    // Scroll update for long destinations (runs frequently, ~50ms)
    // Only run if scrolling is enabled in config
    if (config.scrollEnabled)
    {
        static unsigned long lastScrollCheck = 0;
        if (millis() - lastScrollCheck >= 50)
        {
            lastScrollCheck = millis();
            displayManager.updateScroll();
        }
    }

    // Status logging every 60 seconds
    static unsigned long lastStatusLog = 0;
    if (millis() - lastStatusLog >= 60000)
    {
        lastStatusLog = millis();
        char statusMsg[128];
        snprintf(statusMsg,
                 sizeof(statusMsg),
                 "STATUS: WiFi=%s | AP=%s | Deps=%d | Heap=%u",
                 wifiManager.isConnected() ? "OK" : "FAIL",
                 wifiManager.isAPMode() ? "ON" : "OFF",
                 departureCount,
                 ESP.getFreeHeap());
        logTimestamp();
        debugPrintln(statusMsg);
    }

    // Let idle task run
    delay(1);
}