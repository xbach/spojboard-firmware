#pragma once

// ============================================================================
// AppState — shared application state (Phase 0 of main.cpp decomposition)
// ============================================================================
// Single owner of the firmware's shared, cross-task / cross-module state:
// service objects, the active config, the two FreeRTOS sync primitives, the two
// inter-task request structs, and the API data published for the display.
//
// Concurrency contract (see CLAUDE.md "Thread Safety Rules"):
//   - apiDataMutex protects: departures[]/departureCount, weatherData, infoText,
//     tickerData, stopName, apiError, apiErrorMsg, awaitingDepartures.
//   - displayMutex protects: displayRequest.
// Cross-task booleans are volatile (visibility only; reads/writes are atomic on
// the 32-bit core, compound check-then-act is confined to the loop/web context).
//
// Loop-local bookkeeping (lastDisplayUpdate, lastEtaRecalc, needsDisplayUpdate,
// lastRestCheckMinute) intentionally stays in main.cpp — it is not shared.

#include <Arduino.h>

#include "config/AppConfig.h"
#include "api/DepartureData.h"
#include "api/TransitAPI.h"
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

// ============================================================================
// Service / hardware objects
// ============================================================================
extern DisplayManager displayManager;
extern DisplayController displayController;
extern WiFiManager wifiManager;
extern CaptivePortal captivePortal;
extern ConfigWebServer webServer;
extern GolemioAPI golemioAPI;     // Prague transit API
extern BvgAPI bvgAPI;             // Berlin transit API
extern MqttAPI mqttAPI;           // MQTT transit API
extern TransitAPI* transitAPI;    // Active API (selected at runtime in setup)
extern WeatherAPI weatherAPI;     // Weather forecast API
extern TickerAPI tickerAPI;       // Twelve Data ticker API

// ============================================================================
// Configuration (structure defined in config/AppConfig.h)
// ============================================================================
extern Config config;

// ============================================================================
// Synchronization primitives
// ============================================================================
extern TaskHandle_t displayTaskHandle;
extern SemaphoreHandle_t displayMutex;   // Protects displayRequest
extern TaskHandle_t apiFetchTaskHandle;
extern SemaphoreHandle_t apiDataMutex;   // Protects departures[]/weatherData/infoText/ticker
// Serializes ALL writes to the HUB75 DMA buffer (render task vs. direct draws from
// onAPIStatus/onDemoStop). Leaf lock: never held while taking another mutex.
extern SemaphoreHandle_t displayHwMutex;
// Serializes signalDisplayUpdate() against itself (it is called from both loop() and
// apiFetchTask and uses static snapshot buffers). OUTER lock: taken first, released
// last, wraps the apiDataMutex+displayMutex acquisitions inside.
extern SemaphoreHandle_t signalMutex;
// Guards the `config` struct between its loop-context writer (onConfigSave) and its one
// cross-task reader (apiFetchTask's per-iteration snapshot). Leaf lock: held only for the
// struct copy, never across an HTTP fetch or NVS write.
extern SemaphoreHandle_t configMutex;

// Create all of the above mutexes (call once early in setup(), before display ops or
// tasks). Returns false if any allocation fails (and logs which). displayMutex is created
// first so it exists before the first display operation.
bool createMutexes();

// ============================================================================
// Display update request (protected by displayMutex)
// ============================================================================
struct DisplayUpdateRequest
{
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
    char infoText[TransitAPI::MAX_INFOTEXT_LEN];
    bool tickerMode;
    TickerData ticker;
    bool needsUpdate;
};
extern DisplayUpdateRequest displayRequest;

// ============================================================================
// API fetch request flags (signals from loop to API task)
// ============================================================================
struct APIFetchRequest
{
    volatile bool fetchDeparturesNow;
    volatile bool fetchWeatherNow;
    volatile bool fetchTickerNow;
    unsigned long lastDeparturesFetch;
    unsigned long lastWeatherFetch;
    unsigned long lastTickerFetch;
    // After a failed fetch, retry sooner than the full configured interval (so one miss
    // doesn't blank/stale data for a whole refresh cycle). Touched only by apiFetchTask.
    bool departuresRetryPending;
    bool weatherRetryPending;
    volatile bool timezoneInitialized; // Set true after initTimeSync() - blocks fetches with wrong tz
};
extern APIFetchRequest apiFetchRequest;

// ============================================================================
// Shared API data (protected by apiDataMutex)
// ============================================================================
extern Departure departures[MAX_DEPARTURES];
extern int departureCount;
extern WeatherData weatherData;
extern char infoText[TransitAPI::MAX_INFOTEXT_LEN];
extern TickerData tickerData;
extern char stopName[64];
extern bool apiError;
extern char apiErrorMsg[64];

// ============================================================================
// Cross-task state flags (volatile: written from loop/web callbacks, read from tasks)
// ============================================================================
extern volatile bool demoModeActive;       // Demo mode: stops API polling and display updates
extern volatile bool restModeActive;       // Rest mode: pauses API polling, turns off display
extern volatile bool restModeManual;       // UI LABEL ONLY: panel disagrees with the schedule.
                                          // Never gate on it -- see utils/RestPolicy.h (TA-0254)
extern volatile bool awaitingDepartures;   // True until first fetch completes (shows "Loading")
extern volatile bool tickerModeActive;     // Ticker mode (candlestick chart)
