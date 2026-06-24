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

// Shared application state — service objects, config, mutexes, the two inter-task
// request structs, and the published API data — lives in core/AppState
// (Phase 0 of the main.cpp decomposition, TA-0225).
#include "core/AppState.h"
// Web-server / API callbacks (onConfigSave, onRefresh, onDemo*, onTicker*, …)
// live in core/AppCallbacks (Phase 1 of the decomposition, TA-0225).
#include "core/AppCallbacks.h"
// signalDisplayUpdate() + displayRenderTask() (Phase 2 of the decomposition).
#include "core/DisplayBridge.h"
// apiFetchTask() + recalculateETAs() — the departure pipeline (Phase 3).
#include "core/TransitOrchestrator.h"
// setup()/loop() glue helpers (Phase 4).
#include "core/AppRuntime.h"

// Hardware configuration and defaults are now in config/AppConfig.h
//
// Shared state (service objects, config, mutexes, task handles, the two inter-task
// request structs, published API data, and cross-task flags) is defined in
// core/AppState.cpp and declared in core/AppState.h. main.cpp keeps only the
// loop-local bookkeeping below.

// signalDisplayUpdate() / displayRenderTask() are declared in core/DisplayBridge.h.
// loop()/setup() glue + their timing state now live in core/AppRuntime (Phase 4).


// ============================================================================
// Helper Functions
// ============================================================================

// Check if a given config has valid API configuration. apiFetchTask passes its config
// snapshot (RACE 3 fix); loop/setup use the no-arg overload on the global config.
bool isCityConfigured(const Config& cfg)
{
    if (!cfg.configured)
    {
        return false;
    }

    if (strcmp(cfg.city, "Berlin") == 0)
    {
        // Berlin only needs stop IDs
        return strlen(cfg.berlinStopIds) > 0;
    }
    else if (strcmp(cfg.city, "MQTT") == 0)
    {
        // MQTT needs broker, topics, and field mappings
        return strlen(cfg.mqttBroker) > 0 && strlen(cfg.mqttRequestTopic) > 0 &&
               strlen(cfg.mqttResponseTopic) > 0 && strlen(cfg.mqttFieldLine) > 0 &&
               strlen(cfg.mqttFieldDestination) > 0;
    }
    else
    {
        // Prague needs both API key and stop IDs
        return strlen(cfg.pragueApiKey) > 0 && strlen(cfg.pragueStopIds) > 0;
    }
}

// No-arg overload: evaluate the global config (loop/setup context — same task as the
// config writer, so no snapshot needed).
bool isCityConfigured()
{
    return isCityConfigured(config);
}

// ============================================================================
// Extracted from main.cpp during the TA-0225 decomposition:
//   Phase 1 -> core/AppCallbacks   : onConfigSave/onRefresh/onReboot/onDemo*/
//                                     onRestMode/onTicker*/onAPIStatus
//   Phase 2 -> core/DisplayBridge  : displayRenderTask() + signalDisplayUpdate()
//                                     (BUG 1 + BUG 2 fixes landed there)
//   Phase 3 -> core/TransitOrchestrator : apiFetchTask() + recalculateETAs() +
//                                     attachSecondETAs/publishDepartureSnapshot/AccEntry
// isCityConfigured() (above) stays here — shared by setup()/loop()/the fetch task.
// ============================================================================

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

    // Create all synchronization primitives (display mutex first, before any display op).
    if (!createMutexes())
    {
        while (1)
        {
            delay(1000); // Halt forever
        }
    }

    // Load configuration FIRST (needed for display brightness)
    loadConfig(config);

    // Initialize logger with config for debug mode checks (MUST be after loadConfig)
    initLogger(&config);

    // Select transit API by config.city + wire the status callback
    selectTransitAPI();

    // Initialize display with correct brightness from config
    if (!displayManager.begin(config.brightness, config.panelRows))
    {
        debugPrintln("Display initialization failed!");
        return;
    }
    displayManager.setConfig(&config);
    logMemory("display_init");

    displayManager.drawStatus("Starting SpojBoard...", "FW v" FIRMWARE_RELEASE, COLOR_WHITE);

    // Create display render task on Core 1 (separate from WiFi/network on Core 0).
    // Task stacks MUST live in internal RAM: a task running while the SPI-flash
    // cache is briefly disabled cannot access a PSRAM stack and would panic.
    BaseType_t taskCreated = xTaskCreatePinnedToCore(
        displayRenderTask, "DisplayRender", 12288, NULL, 2, &displayTaskHandle, 1);
    if (taskCreated != pdPASS || displayTaskHandle == NULL)
    {
        Serial.println("FATAL: Failed to create display task!");
        while (1) { delay(1000); }
    }
    delay(100); // Give display task time to start

    // Create API fetch task on Core 1 (handles blocking HTTP calls)
    taskCreated = xTaskCreatePinnedToCore(
        apiFetchTask, "APIFetch", 10240, NULL, 1, &apiFetchTaskHandle, 1);
    if (taskCreated != pdPASS || apiFetchTaskHandle == NULL)
    {
        Serial.println("FATAL: Failed to create API fetch task!");
        while (1) { delay(1000); }
    }
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
        snprintf(ipStr, sizeof(ipStr), "IP: %s", WiFi.localIP().toString().c_str());
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
    pushWebServerState();

    // Skip WiFi monitoring and API calls in AP mode
    if (wifiManager.isAPMode())
    {
        serviceApModeDisplay();
        delay(10);
        return;
    }

    // STA mode: connection monitoring, scheduled rest mode, ETA refresh, display ticks.
    monitorWiFiConnection();
    checkScheduledRestMode();
    serviceEtaRecalc();
    serviceDisplayTicks();

    // Let idle task run
    delay(1);
}
