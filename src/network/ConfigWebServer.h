#ifndef CONFIGWEBSERVER_H
#define CONFIGWEBSERVER_H

#include <WebServer.h>
#include "../config/AppConfig.h"
#include "../api/DepartureData.h"
#include "OTAUpdateManager.h"
#include "GitHubOTA.h"

// Forward declaration
class DisplayManager;

// ============================================================================
// Configuration Web Server
// ============================================================================

/**
 * Web server for device configuration and status display.
 * Provides HTML interface for WiFi, API, and display settings.
 */
class ConfigWebServer
{
  public:
    // Callback types for configuration events
    typedef void (*ConfigSaveCallback)(const Config& newConfig, bool needsRestart, const char* tab);
    typedef void (*RefreshCallback)();
    typedef void (*RebootCallback)();
    typedef void (*DemoStartCallback)(const Departure* demoDepartures, int demoCount);
    typedef void (*DemoStopCallback)();
    typedef void (*RestModeCallback)(bool enabled);
    typedef void (*TickerStartCallback)();
    typedef void (*TickerStopCallback)();
    typedef void (*TickerModeCallback)(bool enabled);
    // Panel colour test (TA-0302). Routed UPWARD rather than drawn here: the
    // display lock lives in the app layer and lower layers never depend on it.
    typedef void (*HwTestCallback)();

    ConfigWebServer();
    ~ConfigWebServer();

    /**
     * Initialize and start web server
     * @return true if server started successfully
     */
    bool begin();

    /**
     * Stop web server
     */
    void stop();

    /**
     * Handle client requests (call from main loop)
     */
    void handleClient();

    /**
     * Set callback functions for configuration events
     */
    void setCallbacks(ConfigSaveCallback onSave,
                      RefreshCallback onRefresh,
                      RebootCallback onReboot,
                      DemoStartCallback onDemoStart = nullptr,
                      DemoStopCallback onDemoStop = nullptr,
                      RestModeCallback onRestMode = nullptr,
                      TickerStartCallback onTickerStart = nullptr,
                      TickerStopCallback onTickerStop = nullptr,
                      TickerModeCallback onTickerMode = nullptr,
                      HwTestCallback onHwTest = nullptr);

    /**
     * Set display manager for OTA progress updates
     * @param displayMgr Pointer to DisplayManager instance
     */
    void setDisplayManager(DisplayManager* displayMgr);

    /**
     * Update current state for status display
     * @param config Current configuration
     * @param wifiConnected WiFi connection status
     * @param apModeActive AP mode status
     * @param apSSID AP network name (if in AP mode)
     * @param apPassword AP password (if in AP mode)
     * @param apClientCount Number of connected AP clients
     * @param apiError API error status
     * @param apiErrorMsg API error message
     * @param departureCount Number of departures
     * @param stopName Current stop name
     * @param demoModeActive Demo mode status
     * @param restModeActive Rest mode status
     * @param restModeManual True if rest mode was manually activated via REST API
     * @param tickerModeActive Ticker mode status
     */
    void updateState(const Config* config,
                     bool wifiConnected,
                     bool apModeActive,
                     const char* apSSID,
                     const char* apPassword,
                     int apClientCount,
                     bool apiError,
                     const char* apiErrorMsg,
                     int departureCount,
                     const char* stopName,
                     bool demoModeActive,
                     bool restModeActive,
                     bool restModeManual,
                     bool tickerModeActive = false);

    /**
     * Get web server instance for direct access
     */
    WebServer* getServer()
    {
        return server;
    }

  private:
    WebServer* server;
    OTAUpdateManager* otaManager;
    GitHubOTA* githubOTA;
    DisplayManager* displayManager;

    // Current state (for status display)
    const Config* currentConfig;
    bool wifiConnected;
    bool apModeActive;
    const char* apSSID;
    const char* apPassword;
    int apClientCount;
    bool apiError;
    const char* apiErrorMsg;
    int departureCount;
    const char* stopName;
    bool demoModeActive;
    bool restModeActive;
    bool restModeManual;
    bool tickerModeActive;

    // Callbacks
    ConfigSaveCallback onSaveCallback;
    RefreshCallback onRefreshCallback;
    RebootCallback onRebootCallback;
    DemoStartCallback onDemoStartCallback;
    DemoStopCallback onDemoStopCallback;
    RestModeCallback onRestModeCallback;
    HwTestCallback onHwTestCallback;
    TickerStartCallback onTickerStartCallback;
    TickerStopCallback onTickerStopCallback;
    TickerModeCallback onTickerModeCallback;

    // HTTP handlers
    void handleRoot();
    void handleSave();
    void handleRefresh();
    void handleReboot();
    void handleClearConfig(); // POST: clear all settings (factory reset), requires confirm=RESET
    void handleConfigExport(); // GET: download the whole config as JSON (TA-0307)
    void handleConfigImport(); // POST: apply a config JSON body, then reboot (TA-0307)
    void handleUpdate(); // GET: show OTA upload form
    void handleUpdateProgress(); // POST: handle firmware upload chunks
    void handleUpdateComplete(); // POST: handle firmware upload completion
    void handleCheckUpdate(); // GET: check GitHub for updates (AJAX)
    void handleDownloadUpdate(); // POST: download and install from GitHub (AJAX)
    void handleDemo(); // GET: show demo configuration page
    void handleStartDemo(); // POST: start demo mode with sample data
    void handleStopDemo(); // POST: stop demo mode and resume normal operation
    void handleRestMode(); // POST: control rest mode via REST API
    void handleTicker(); // GET: show ticker configuration page
    void handleStartTicker(); // POST: save settings and start ticker mode
    void handleStopTicker(); // POST: stop ticker mode
    void handleTickerMode(); // POST: REST API toggle for ticker mode
    void handleDepartures(); // GET: show cached departure data page
    void handleDeparturesData(); // GET: return cached departure data as JSON (AJAX)
    void handleInfoText(); // GET: show infotext test page
    void handleSetInfoText(); // POST: set custom infotext on display
    void handleClearInfoText(); // POST: clear custom infotext
    void handleCurrentInfoText(); // GET: current infotext state
    void handleNotFound();

    // Config parsing helpers — one per tab for per-tab save dispatch
    void parseWifiSettings(Config* config, bool* wifiChanged);
    void parseConnectionSettings(Config* config, bool* cityChanged);
    void parseTransitSettings(Config* config);
    void parsePragueSettings(Config* config);
    void parseBerlinSettings(Config* config);
    void parseMqttSettings(Config* config);
    void parseDisplaySettings(Config* config);
    void parseHardwareSettings(Config* config); // panel wiring (TA-0302)
    void handleHwTest();
    void handleResetDisplayPins();
    void parseOptionalSettings(Config* config);

    // OTA progress callbacks (static for use as function pointers)
    static void otaProgressCallback(size_t progress, size_t total);
    static void githubOtaProgressCallback(size_t progress, size_t total);
    static ConfigWebServer* instanceForCallback; // Static instance pointer for callbacks
};

#endif // CONFIGWEBSERVER_H
