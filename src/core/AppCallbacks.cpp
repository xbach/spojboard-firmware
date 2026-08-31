// ============================================================================
// AppCallbacks — definitions (Phase 1 of main.cpp decomposition)
// ============================================================================

#include "core/AppCallbacks.h"

#include "core/AppState.h"
#include "core/DisplayBridge.h"   // signalDisplayUpdate()
#include "core/AppRuntime.h"     // applyRestDecision (TA-0254)
#include "utils/RestPolicy.h"    // resolveManual
#include "display/DisplayColors.h" // COLOR_YELLOW
#include "utils/Logger.h"

// ============================================================================
// API Status Callback - Updates display during retries
// ============================================================================
void onAPIStatus(const char* message)
{
    // Runs in apiFetchTask context. BUG 2 fix (TA-0225): take displayHwMutex so this
    // direct draw can't tear the frame against displayRenderTask's render(). Leaf lock.
    xSemaphoreTake(displayHwMutex, portMAX_DELAY);
    displayManager.drawStatus(message, "", COLOR_YELLOW);
    xSemaphoreGive(displayHwMutex);
}

// ============================================================================
// Callback Functions for ConfigWebServer
// ============================================================================

void onConfigSave(const Config& newConfig, bool needsRestart, const char* tab)
{
    // Update config. RACE 3 fix (TA-0225): the struct copy is the multi-byte write that
    // apiFetchTask's snapshot could otherwise read torn — guard it with configMutex.
    // saveConfig (NVS write) runs outside the lock: config has no other writer, and
    // apiFetchTask only reads, so the lock is held just for the in-memory copy.
    xSemaphoreTake(configMutex, portMAX_DELAY);
    config = newConfig;
    xSemaphoreGive(configMutex);
    saveConfig(config);

    // Apply brightness immediately
    displayManager.setBrightness(config.brightness);

    if (needsRestart)
    {
        // Restart to apply the settings that are only read at boot
        delay(1000);
        ESP.restart();
    }
    else
    {
        // A saved schedule must apply now, not at the next :00/:30. Idempotent:
        // with no flip the evaluation does nothing, so an unrelated save cannot
        // cancel a manual override (utils/RestPolicy.h).
        requestRestModeReevaluation();

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

void onHwTestPattern()
{
    // Loop context (web handler). Direct DMA-buffer write, so it must be
    // serialized against the render task. Blocking is fine here: the user asked
    // for this and expects the panel to change.
    xSemaphoreTake(displayHwMutex, portMAX_DELAY);
    displayManager.drawColorTest();
    xSemaphoreGive(displayHwMutex);

    // Deliberately NOT signalDisplayUpdate(): the pattern should stay up until
    // something else repaints, so the user can look at it.
}

void onDemoStop()
{
    // Exit demo mode: restore previous state (rest mode or normal operation)
    demoModeActive = false;

    // Check if rest mode was active before demo started (manual or scheduled)
    if (restModeActive)
    {
        // Return to rest mode (was active before demo, stays active).
        // BUG 2 fix (TA-0225): serialize this direct DMA-buffer write against the
        // render task via displayHwMutex (leaf lock, released before signalDisplayUpdate).
        xSemaphoreTake(displayHwMutex, portMAX_DELAY);
        displayManager.getDisplay()->clearScreen();
        displayManager.getDisplay()->flipDMABuffer();
        xSemaphoreGive(displayHwMutex);
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
    // Thin adapter. All arbitration lives in utils/RestPolicy.h; all side effects
    // live in applyRestDecision(). This used to open-code both, and its two guards
    //     if (enabled && !restModeActive) ... else if (!enabled && restModeActive)
    // matched NEITHER branch during a scheduled rest -- the button was a silent
    // no-op with no way to wake the panel. resolveManual() has no such hole: a
    // press always produces a decision.
    //
    // The scheduler's opinion is deliberately NOT passed back in: a press is a
    // disagreement, not a lesson. It is read only to decide whether this press
    // counts as an override for the UI label.
    const RestDecision d = resolveManual(restModeActive, enabled, currentScheduleOpinion());

    if (d.changed)
    {
        logTimestamp();
        debugPrintln(d.restActive
                         ? "RestMode: Activated via REST API - display off, API polling paused"
                         : "RestMode: Deactivated via REST API - resuming normal operation");
    }

    applyRestDecision(d);
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
