// ============================================================================
// DisplayBridge — definitions (Phase 2 of main.cpp decomposition)
// ============================================================================

#include "core/DisplayBridge.h"

#include "core/AppState.h"
#include "utils/Logger.h"

// Defined in main.cpp (moves to core/TransitOrchestrator in Phase 3).
bool isCityConfigured();

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
            char localInfoText[TransitAPI::MAX_INFOTEXT_LEN];
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
                strlcpy(localInfoText, displayRequest.infoText, sizeof(localInfoText));
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

            // Stage the per-frame data (stored by value, thread-safe). These are
            // member stores, not DMA writes, and only this task touches them.
            displayManager.setWeatherData(&localWeather);
            displayManager.setInfoText(localInfoText);

            logTimestamp();
            debugPrintln("DisplayTask: Rendering on Core 1");

            // BUG 2 fix (TA-0225): serialize all HUB75 DMA-buffer writes. displayHwMutex
            // is a leaf lock — displayMutex was already released above, so nothing else is
            // held here. This blocks onAPIStatus/onDemoStop (other tasks) from drawing into
            // the same buffer mid-render, which previously tore the frame / risked DMA
            // corruption. Priority inheritance keeps this prio-2 task from being starved.
            xSemaphoreTake(displayHwMutex, portMAX_DELAY);
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
            xSemaphoreGive(displayHwMutex);

            logTimestamp();
            debugPrintln("DisplayTask: Render complete");
        }

        // Small yield to prevent task starvation
        taskYIELD();
    }
}

// ============================================================================
// signalDisplayUpdate — snapshot shared state and notify the render task
// ============================================================================
// Called from BOTH loop() AND apiFetchTask (via publishDepartureSnapshot / weather /
// ticker updates). The static snapshot buffers below are shared between those two
// prio-1 Core-1 callers, so the whole body is serialized by signalMutex (Race 5 fix,
// TA-0225): an OUTER lock, always taken first and released last. The inner
// apiDataMutex / displayMutex are acquired and released within, so the lock order is
// acyclic (no deadlock). portMAX_DELAY → never drop an update; the body is bounded
// (the inner takes time out at 50ms). Static buffers stay in .bss — localDeps (~3KB)
// would risk overflowing apiFetchTask's 10KB stack at its deep call site.
void signalDisplayUpdate()
{
    xSemaphoreTake(signalMutex, portMAX_DELAY);

    // Snapshot API-protected data first (departures, weather, ticker, infotexts).
    // Static to keep these large buffers off the (10-12KB) task stacks.
    static Departure localDeps[MAX_DEPARTURES];
    static TickerData localTicker;
    static char localInfoText[TransitAPI::MAX_INFOTEXT_LEN];
    int localDepCount = 0;
    WeatherData localWeather = {};
    memset(&localTicker, 0, sizeof(localTicker));
    localInfoText[0] = '\0';

    // BUG 1 fix (TA-0225): apiError/apiErrorMsg/stopName are apiDataMutex-protected
    // (written by publishDepartureSnapshot). They MUST be snapshotted here, under
    // apiDataMutex — not read later under displayMutex, which would be a torn read of
    // the char[64] buffers concurrently with the fetch task's strlcpy.
    bool localApiError = false;
    char localApiErrorMsg[64] = "";
    char localStopName[64] = "";

    if (xSemaphoreTake(apiDataMutex, pdMS_TO_TICKS(50)))
    {
        memcpy(localDeps, departures, sizeof(Departure) * departureCount);
        localDepCount = departureCount;
        localWeather = weatherData;
        localTicker = tickerData;
        strlcpy(localInfoText, infoText, sizeof(localInfoText));
        localApiError = apiError;
        strlcpy(localApiErrorMsg, apiErrorMsg, sizeof(localApiErrorMsg));
        strlcpy(localStopName, stopName, sizeof(localStopName));
        xSemaphoreGive(apiDataMutex);
    }
    else
    {
        logTimestamp();
        debugPrintln("signalDisplayUpdate: apiDataMutex timeout, update skipped");
        xSemaphoreGive(signalMutex);
        return;
    }

    if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(50)))
    {
        // Copy API data snapshot
        memcpy(displayRequest.departures, localDeps,
               sizeof(Departure) * localDepCount);
        displayRequest.departureCount = localDepCount;
        displayRequest.weather = localWeather;
        displayRequest.ticker = localTicker;
        strlcpy(displayRequest.infoText, localInfoText, sizeof(displayRequest.infoText));

        // apiData-protected fields: assigned from the apiDataMutex snapshot above
        // (BUG 1 fix), NOT read from the globals here under only displayMutex.
        displayRequest.apiError = localApiError;
        strlcpy(displayRequest.apiErrorMsg, localApiErrorMsg, sizeof(displayRequest.apiErrorMsg));
        strlcpy(displayRequest.stopName, localStopName, sizeof(displayRequest.stopName));

        // Copy non-API state (safe to read from loop/API context)
        displayRequest.numDepartures = config.numDepartures;
        displayRequest.wifiConnected = wifiManager.isConnected();
        displayRequest.apMode = wifiManager.isAPMode();
        strlcpy(displayRequest.apSSID, wifiManager.getAPSSID(), sizeof(displayRequest.apSSID));
        strlcpy(displayRequest.apPassword, wifiManager.getAPPassword(), sizeof(displayRequest.apPassword));
        displayRequest.cityConfigured = isCityConfigured();
        displayRequest.demoMode = demoModeActive;
        displayRequest.restMode = restModeActive;
        displayRequest.departuresLoading = awaitingDepartures;
        displayRequest.tickerMode = tickerModeActive;
        displayRequest.needsUpdate = true;

        xSemaphoreGive(displayMutex);

        // Notify display task to wake up and render
        xTaskNotify(displayTaskHandle, 1, eSetValueWithOverwrite);
    }
    else
    {
        logTimestamp();
        debugPrintln("signalDisplayUpdate: displayMutex timeout, update skipped");
    }

    xSemaphoreGive(signalMutex);
}
