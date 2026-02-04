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
#include "display/DisplayManager.h"
#include "network/WiFiManager.h"
#include "network/CaptivePortal.h"
#include "network/ConfigWebServer.h"

// Hardware configuration and defaults are now in config/AppConfig.h

// ============================================================================
// Global Objects
// ============================================================================
DisplayManager displayManager;
WiFiManager wifiManager;
CaptivePortal captivePortal;
ConfigWebServer webServer;
GolemioAPI golemioAPI; // Prague transit API
BvgAPI bvgAPI; // Berlin transit API
MqttAPI mqttAPI; // MQTT transit API
TransitAPI* transitAPI = nullptr; // Pointer to active API (selected at runtime)
WeatherAPI weatherAPI; // Weather forecast API

// ============================================================================
// Configuration Storage (structure defined in config/AppConfig.h)
// ============================================================================
Config config;

// ============================================================================
// Departure Data (structure defined in api/DepartureData.h)
// ============================================================================
Departure departures[MAX_DEPARTURES];
int departureCount = 0;

// ============================================================================
// State Variables
// ============================================================================
unsigned long lastApiCall = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastEtaRecalc = 0; // For 10-second ETA recalculation
unsigned long lastWeatherCall = 0; // For weather API polling
bool needsDisplayUpdate = false;
bool apiError = false;
char apiErrorMsg[64] = "";
char stopName[64] = "";
bool demoModeActive = false; // Demo mode flag - stops API polling and display updates
bool restModeActive = false; // Rest mode flag - pauses API polling and turns off display
bool restModeManual = false; // True if rest mode was activated via REST API (skip periodic check)
int lastRestCheckMinute = -1; // Last minute when rest check triggered (0-59)
WeatherData weatherData = {}; // Global weather state

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
// ETA Recalculation - Updates ETAs from cached timestamps every 10s
// ============================================================================
void recalculateETAs()
{
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

        // Filter: Keep only departures that meet minimum departure time threshold
        // (applies to all APIs including MQTT - filters during recalculation)
        if (eta > 0 && eta >= config.minDepartureTime)
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

    // Reset scroll state since departures may have changed positions
    displayManager.resetScroll();

    logTimestamp();
    debugPrintln("ETA Recalc: Complete, display update triggered");
    needsDisplayUpdate = true;
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
// API Fetch Wrapper - Uses TransitAPI interface (GolemioAPI or BvgAPI)
// ============================================================================
void fetchDepartures()
{
    if (!wifiManager.isConnected() || transitAPI == nullptr)
    {
        return;
    }

    // Call API client
    TransitAPI::APIResult result = transitAPI->fetchDepartures(config);

    // Update global state with results
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

    needsDisplayUpdate = true;
}

// ============================================================================
// Weather API Fetch Wrapper
// ============================================================================

void fetchWeather()
{
    if (!wifiManager.isConnected() || !config.weatherEnabled)
    {
        return;
    }

    // Validate coordinates (non-zero)
    if (config.weatherLatitude == 0.0 && config.weatherLongitude == 0.0)
    {
        return;
    }

    logTimestamp();
    debugPrintln("Weather: Fetching forecast...");

    weatherData = weatherAPI.fetchWeather(config.weatherLatitude, config.weatherLongitude);

    if (weatherData.hasError)
    {
        logTimestamp();
        debugPrint("Weather: Error - ");
        debugPrintln(weatherData.errorMsg);
    }
    else
    {
        logTimestamp();
        char msg[64];
        snprintf(msg, sizeof(msg), "Weather: %d°C, code %d", weatherData.temperature, weatherData.weatherCode);
        debugPrintln(msg);
    }

    needsDisplayUpdate = true;
}

// ============================================================================
// Callback Functions for ConfigWebServer
// ============================================================================

void onConfigSave(const Config& newConfig, bool wifiChanged)
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
        // Trigger immediate API refresh
        lastApiCall = 0;
    }
}

void onRefresh()
{
    lastApiCall = 0; // Force immediate refresh
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

    // Copy demo departures to global state
    departureCount = (demoCount > MAX_DEPARTURES) ? MAX_DEPARTURES : demoCount;
    for (int i = 0; i < departureCount; i++)
    {
        departures[i] = demoDepartures[i];
    }

    // Trigger display update with demo data
    needsDisplayUpdate = true;

    logTimestamp();
    debugPrintln("Demo mode activated - API polling stopped");
}

void onDemoStop()
{
    // Exit demo mode: check if we should return to rest mode
    demoModeActive = false;

    // Check if we're in a rest period
    if (isInRestPeriod(config.restModePeriods))
    {
        // Return to rest mode
        restModeActive = true;
        displayManager.getDisplay()->clearScreen();
        displayManager.getDisplay()->flipDMABuffer();

        logTimestamp();
        debugPrintln("Demo stopped - returning to rest mode (display off)");
    }
    else
    {
        // Resume normal operation
        lastApiCall = 0;       // Force immediate API refresh
        lastWeatherCall = 0;   // Force weather refresh

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
        displayManager.getDisplay()->clearScreen();
        displayManager.getDisplay()->flipDMABuffer();

        logTimestamp();
        debugPrintln("RestMode: Activated via REST API - display off, API polling paused");
    }
    else if (!enabled && restModeActive)
    {
        // Exit rest mode
        restModeActive = false;
        restModeManual = false; // Clear manual flag
        lastApiCall = 0;       // Force immediate API refresh
        lastWeatherCall = 0;   // Force weather refresh
        needsDisplayUpdate = true;

        logTimestamp();
        debugPrintln("RestMode: Deactivated via REST API - resuming normal operation");
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

        // Start telnet logger if debug mode enabled
        if (config.debugMode)
        {
            TelnetLogger::getInstance().begin(23);
            logTimestamp();
            debugPrintln("Debug mode enabled - telnet logging active");
        }
    }

    // Initialize web server with callbacks
    webServer.setCallbacks(onConfigSave, onRefresh, onReboot, onDemoStart, onDemoStop, onRestMode);
    webServer.setDisplayManager(&displayManager); // For OTA progress updates
    if (!webServer.begin())
    {
        debugPrintln("Web server failed to start!");
    }

    // Pass weather data pointer to display manager
    displayManager.setWeatherData(&weatherData);

    // Setup captive portal detection handlers
    if (wifiManager.isAPMode())
    {
        captivePortal.setupDetectionHandlers(webServer.getServer());
    }

    // Setup NTP time if connected to WiFi
    if (wifiManager.isConnected() && !wifiManager.isAPMode())
    {
        logTimestamp();
        debugPrintln("Syncing time...");
        initTimeSync();

        if (syncTime(10, 500))
        {
            char timeStr[32];
            if (getFormattedTime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S"))
            {
                char msg[64];
                snprintf(msg, sizeof(msg), "Time synced: %s", timeStr);
                logTimestamp();
                debugPrintln(msg);
            }
        }

        // Initial API call if configured
        if (isCityConfigured())
        {
            fetchDepartures();
            lastApiCall = millis(); // Prevent immediate second call in loop()
        }

        // Initial weather call if configured
        if (config.weatherEnabled && config.weatherLatitude != 0.0 && config.weatherLongitude != 0.0)
        {
            fetchWeather();
            lastWeatherCall = millis();
        }
    }

    needsDisplayUpdate = true;
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
                          restModeManual);

    // Skip WiFi monitoring and API calls in AP mode
    if (wifiManager.isAPMode())
    {
        // Update display periodically in AP mode
        if (millis() - lastDisplayUpdate >= 5000)
        {
            lastDisplayUpdate = millis();
            needsDisplayUpdate = true;
        }

        if (needsDisplayUpdate)
        {
            needsDisplayUpdate = false;
            displayManager.updateDisplay(departures,
                                         departureCount,
                                         config.numDepartures,
                                         wifiManager.isConnected(),
                                         wifiManager.isAPMode(),
                                         wifiManager.getAPSSID(),
                                         wifiManager.getAPPassword(),
                                         apiError,
                                         apiErrorMsg,
                                         stopName,
                                         isCityConfigured(),
                                         demoModeActive);
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
        needsDisplayUpdate = true;

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
        needsDisplayUpdate = true;
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
                    displayManager.getDisplay()->clearScreen();
                    displayManager.getDisplay()->flipDMABuffer();
                    logTimestamp();
                    debugPrintln("RestMode: Activated (scheduled) - display off, API polling paused");
                }
                else if (!shouldBeInRest && restModeActive)
                {
                    // Exit rest mode (scheduled period ended)
                    restModeActive = false;
                    lastApiCall = 0;
                    lastWeatherCall = 0;
                    needsDisplayUpdate = true;
                    logTimestamp();
                    debugPrintln("RestMode: Deactivated (scheduled) - resuming normal operation");
                }
            }
        }
    }

    // Skip API polling and ETA recalculation in demo mode or rest mode
    if (!demoModeActive && !restModeActive)
    {
        // Periodic API calls (only when connected and not in AP mode)
        if (wifiManager.isConnected() && isCityConfigured())
        {
            unsigned long now = millis();
            unsigned long interval = (unsigned long)config.refreshInterval * 1000;

            if (now - lastApiCall >= interval || lastApiCall == 0)
            {
                lastApiCall = now;
                fetchDepartures();
            }
        }

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

        // Periodic weather calls (only when connected and enabled)
        if (wifiManager.isConnected() && config.weatherEnabled && config.weatherLatitude != 0.0 &&
            config.weatherLongitude != 0.0)
        {
            unsigned long now = millis();
            unsigned long weatherInterval = (unsigned long)config.weatherRefreshInterval * 60000; // Minutes to ms

            if (now - lastWeatherCall >= weatherInterval || lastWeatherCall == 0)
            {
                lastWeatherCall = now;
                fetchWeather();
            }
        }
    }

    // Update display (skip if in rest mode)
    if (needsDisplayUpdate && !restModeActive)
    {
        needsDisplayUpdate = false;
        displayManager.updateDisplay(departures,
                                     departureCount,
                                     config.numDepartures,
                                     wifiManager.isConnected(),
                                     wifiManager.isAPMode(),
                                     wifiManager.getAPSSID(),
                                     wifiManager.getAPPassword(),
                                     apiError,
                                     apiErrorMsg,
                                     stopName,
                                     isCityConfigured(),
                                     demoModeActive);
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