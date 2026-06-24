// ============================================================================
// AppRuntime — definitions (Phase 4 of main.cpp decomposition)
// ============================================================================
// Bodies moved verbatim from main.cpp's setup()/loop(); only wrapped in functions.

#include "core/AppRuntime.h"

#include "core/AppState.h"
#include "core/AppCallbacks.h"        // onAPIStatus
#include "core/DisplayBridge.h"       // signalDisplayUpdate
#include "core/TransitOrchestrator.h" // recalculateETAs
#include "utils/Logger.h"
#include "utils/TimeUtils.h" // getCurrentTime
#include "utils/RestMode.h"  // isInRestPeriod

// Loop-local bookkeeping (moved from main.cpp — used only by these helpers).
static unsigned long lastDisplayUpdate = 0;
static unsigned long lastEtaRecalc = 0; // For 10-second ETA recalculation
static bool needsDisplayUpdate = false;
static int lastRestCheckMinute = -1; // Last minute when rest check triggered (0-59)

// ============================================================================
// setup() helpers
// ============================================================================
void selectTransitAPI()
{
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
}

// ============================================================================
// loop() helpers
// ============================================================================
void pushWebServerState()
{
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
}

void serviceApModeDisplay()
{
    // Update display periodically in AP mode
    if (millis() - lastDisplayUpdate >= 5000)
    {
        lastDisplayUpdate = millis();
        signalDisplayUpdate(); // Signal Core 1 display task to render
    }

    if (needsDisplayUpdate)
    {
        needsDisplayUpdate = false;
        signalDisplayUpdate(); // Signal Core 1 display task to render
    }
}

void monitorWiFiConnection()
{
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
}

void checkScheduledRestMode()
{
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
}

void serviceEtaRecalc()
{
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
}

void serviceDisplayTicks()
{
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

    // Infotext alternation update (runs frequently, ~50ms)
    {
        static unsigned long lastInfoTextCheck = 0;
        if (millis() - lastInfoTextCheck >= 50)
        {
            lastInfoTextCheck = millis();
            displayManager.updateInfoText();
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
}
