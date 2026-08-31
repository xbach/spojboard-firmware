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
#include "utils/RestMode.h"   // isInRestPeriod
#include "utils/RestPolicy.h" // resolveSchedule / resolveManual (TA-0254)

// Loop-local bookkeeping (moved from main.cpp — used only by these helpers).
static unsigned long lastDisplayUpdate = 0;
static unsigned long lastEtaRecalc = 0; // For 10-second ETA recalculation
static bool needsDisplayUpdate = false;
// -1 doubles as "evaluate at the next opportunity": at boot, and after a config
// save. An edge-triggered schedule cannot assert itself until it FLIPS, and a flip
// cannot happen before the first evaluation -- so without this the panel keeps
// whatever state it booted into until the clock next reaches :00 or :30.
#define REST_CHECK_IMMEDIATE (-1)
static int lastRestCheckMinute = REST_CHECK_IMMEDIATE; // Last minute checked (0-59)

// The schedule's last opinion. Edge-triggering is the whole design: see
// utils/RestPolicy.h. Nothing outside resolveSchedule() may interpret this.
static int8_t lastScheduleOpinion = SCHEDULE_UNKNOWN;

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

void applyRestDecision(const RestDecision& d)
{
    // The label can move without the panel moving (pressing for the state the
    // schedule already wants), so both flags are assigned either way.
    restModeActive = d.restActive;
    restModeManual = d.manual;

    // Only a real state change has side effects. `manual` is a web-UI label; it
    // does not touch the panel, so it must not trigger a redraw or a refetch.
    if (!d.changed)
        return;

    if (!d.restActive)
    {
        // WAKING IS MORE THAN BRIGHTNESS. API polling paused while we slept, so
        // whatever feeds the panel has to be restarted or the board comes back
        // showing stale rows that never refresh. This lived open-coded in BOTH
        // the scheduler and the manual handler; it is here once precisely so a
        // future wake path cannot restore the light and forget the data.
        displayManager.setBrightness(config.brightness);

        if (!tickerModeActive)
        {
            awaitingDepartures = true; // Show loading state until fresh data arrives
            apiFetchRequest.fetchDeparturesNow = true;
        }
        else
        {
            apiFetchRequest.fetchTickerNow = true;
        }
        apiFetchRequest.fetchWeatherNow = true; // Always (status bar)
    }

    signalDisplayUpdate();
}

int8_t currentScheduleOpinion()
{
    return lastScheduleOpinion;
}

void requestRestModeReevaluation()
{
    // Cheap and idempotent: with no flip resolveSchedule() does nothing, so a
    // manual override survives an unrelated config save. A save that CHANGES the
    // rest windows does flip it, and the schedule correctly takes back control.
    lastRestCheckMinute = REST_CHECK_IMMEDIATE;
}

void checkScheduledRestMode()
{
    // NOTE: no `!restModeManual` gate. There used to be one, to stop a
    // level-triggered scheduler from immediately undoing a manual press -- and it
    // meant one press disarmed the schedule until the next press. Edge-triggering
    // removes the need, so re-adding any gate here reintroduces that bug.
    if (wifiManager.isAPMode())
        return;

    struct tm timeinfo;
    if (!getCurrentTime(&timeinfo))
        return;

    const int currentMinute = timeinfo.tm_min;
    const bool immediate = (lastRestCheckMinute == REST_CHECK_IMMEDIATE);
    const bool atCheckpoint =
        (currentMinute == 0 || currentMinute == 30) && currentMinute != lastRestCheckMinute;

    if (!immediate && !atCheckpoint)
        return;

    lastRestCheckMinute = currentMinute;

    const bool restNow = isInRestPeriod(config.restModePeriods);
    const RestDecision d =
        resolveSchedule(restModeActive, restModeManual, lastScheduleOpinion, restNow);
    lastScheduleOpinion = d.opinion;

    if (d.changed)
    {
        logTimestamp();
        debugPrintln(d.restActive
                         ? "RestMode: Activated (scheduled) - display off, API polling paused"
                         : "RestMode: Deactivated (scheduled) - resuming normal operation");
    }

    applyRestDecision(d);
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
