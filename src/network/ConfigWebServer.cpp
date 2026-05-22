#include "ConfigWebServer.h"
#include "../utils/Logger.h"
#include "../display/DisplayManager.h"
#include "../api/WeatherAPI.h"
#include "web/WebTemplates.h"
#include "web/DashboardPage.h"
#include "web/DemoPage.h"
#include "web/InfoTextPage.h"
#include "web/TickerPage.h"
#include "web/DeparturesPage.h"
#include "web/UpdatePage.h"
#include "web/ApiHandlers.h"
#include "web/WebUtils.h"
#include <WiFi.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <string.h>

// Static instance pointer for OTA callback
ConfigWebServer *ConfigWebServer::instanceForCallback = nullptr;

ConfigWebServer::ConfigWebServer()
    : server(nullptr), otaManager(nullptr), githubOTA(nullptr), displayManager(nullptr),
      currentConfig(nullptr),
      wifiConnected(false), apModeActive(false),
      apSSID(""), apPassword(""), apClientCount(0),
      apiError(false), apiErrorMsg(""), departureCount(0), stopName(""),
      demoModeActive(false), restModeActive(false), restModeManual(false), tickerModeActive(false),
      onSaveCallback(nullptr), onRefreshCallback(nullptr), onRebootCallback(nullptr),
      onDemoStartCallback(nullptr), onDemoStopCallback(nullptr), onRestModeCallback(nullptr),
      onTickerStartCallback(nullptr), onTickerStopCallback(nullptr), onTickerModeCallback(nullptr)
{
    otaManager = new OTAUpdateManager();
    githubOTA = new GitHubOTA();
    instanceForCallback = this;
}

ConfigWebServer::~ConfigWebServer()
{
    stop();

    if (otaManager != nullptr)
    {
        delete otaManager;
        otaManager = nullptr;
    }

    if (githubOTA != nullptr)
    {
        delete githubOTA;
        githubOTA = nullptr;
    }
}

bool ConfigWebServer::begin()
{
    if (server != nullptr)
    {
        return true; // Already started
    }

    server = new WebServer(80);

    // Register handlers with lambda wrappers to access 'this'
    server->on("/", HTTP_GET, [this]()
               { handleRoot(); });
    server->on("/save", HTTP_POST, [this]()
               { handleSave(); });
    server->on("/refresh", HTTP_POST, [this]()
               { handleRefresh(); });
    server->on("/reboot", HTTP_POST, [this]()
               { handleReboot(); });
    server->on("/clear-config", HTTP_POST, [this]()
               { handleClearConfig(); });
    server->on("/update", HTTP_GET, [this]()
               { handleUpdate(); });
    server->on("/update", HTTP_POST,
               [this]() { handleUpdateComplete(); },  // Completion handler
               [this]() { handleUpdateProgress(); }   // Upload chunk handler
    );
    server->on("/check-update", HTTP_GET, [this]()
               { handleCheckUpdate(); });
    server->on("/download-update", HTTP_POST, [this]()
               { handleDownloadUpdate(); });
    server->on("/demo", HTTP_GET, [this]()
               { handleDemo(); });
    server->on("/start-demo", HTTP_POST, [this]()
               { handleStartDemo(); });
    server->on("/stop-demo", HTTP_POST, [this]()
               { handleStopDemo(); });
    server->on("/rest-mode", HTTP_POST, [this]()
               { handleRestMode(); });
    server->on("/ticker", HTTP_GET, [this]()
               { handleTicker(); });
    server->on("/start-ticker", HTTP_POST, [this]()
               { handleStartTicker(); });
    server->on("/stop-ticker", HTTP_POST, [this]()
               { handleStopTicker(); });
    server->on("/ticker-mode", HTTP_POST, [this]()
               { handleTickerMode(); });
    server->on("/departures", HTTP_GET, [this]()
               { handleDepartures(); });
    server->on("/departures-data", HTTP_GET, [this]()
               { handleDeparturesData(); });
    server->on("/infotext", HTTP_GET, [this]()
               { handleInfoText(); });
    server->on("/set-infotext", HTTP_POST, [this]()
               { handleSetInfoText(); });
    server->on("/clear-infotext", HTTP_POST, [this]()
               { handleClearInfoText(); });
    server->on("/current-infotext", HTTP_GET, [this]()
               { handleCurrentInfoText(); });
    server->onNotFound([this]()
                       { handleNotFound(); });

    server->begin();

    // Initialize OTA manager
    if (otaManager != nullptr)
    {
        otaManager->begin();
    }

    logTimestamp();
    Serial.println("Web server started on port 80");

    return true;
}

void ConfigWebServer::stop()
{
    if (server != nullptr)
    {
        server->stop();
        delete server;
        server = nullptr;

        logTimestamp();
        Serial.println("Web server stopped");
    }
}

void ConfigWebServer::handleClient()
{
    if (server != nullptr)
    {
        server->handleClient();
    }
}

void ConfigWebServer::setCallbacks(ConfigSaveCallback onSave, RefreshCallback onRefresh, RebootCallback onReboot,
                                  DemoStartCallback onDemoStart, DemoStopCallback onDemoStop,
                                  RestModeCallback onRestMode,
                                  TickerStartCallback onTickerStart, TickerStopCallback onTickerStop,
                                  TickerModeCallback onTickerMode)
{
    onSaveCallback = onSave;
    onRefreshCallback = onRefresh;
    onRebootCallback = onReboot;
    onDemoStartCallback = onDemoStart;
    onDemoStopCallback = onDemoStop;
    onRestModeCallback = onRestMode;
    onTickerStartCallback = onTickerStart;
    onTickerStopCallback = onTickerStop;
    onTickerModeCallback = onTickerMode;
}

void ConfigWebServer::setDisplayManager(DisplayManager *displayMgr)
{
    displayManager = displayMgr;
}

void ConfigWebServer::updateState(const Config *config,
                                  bool connected, bool apMode,
                                  const char *ssid, const char *password, int clientCount,
                                  bool error, const char *errorMsg,
                                  int depCount, const char *stop,
                                  bool demoMode, bool restMode, bool restManual,
                                  bool tickerMode)
{
    currentConfig = config;
    wifiConnected = connected;
    apModeActive = apMode;
    apSSID = ssid;
    apPassword = password;
    apClientCount = clientCount;
    apiError = error;
    apiErrorMsg = errorMsg;
    departureCount = depCount;
    stopName = stop;
    demoModeActive = demoMode;
    restModeActive = restMode;
    restModeManual = restManual;
    tickerModeActive = tickerMode;
}

void ConfigWebServer::handleRoot()
{
    if (currentConfig == nullptr)
    {
        server->send(500, "text/plain", "Server not initialized");
        return;
    }

    sendDashboardPage(
        server,
        currentConfig,
        apModeActive,
        wifiConnected,
        apSSID,
        apClientCount,
        apiError,
        apiErrorMsg,
        departureCount,
        stopName,
        demoModeActive,
        restModeActive,
        restModeManual
    );
}

void ConfigWebServer::handleSave()
{
    if (currentConfig == nullptr || onSaveCallback == nullptr)
    {
        server->send(500, "text/plain", "Server not initialized");
        return;
    }

    // Create a copy of config to modify
    Config newConfig = *currentConfig;
    bool wifiChanged = false;
    bool cityChanged = false;
    bool displaySizeChanged = false;

    // Determine which tab is being saved (default "all" for AP mode / backward compat)
    String tab = server->hasArg("tab") ? server->arg("tab") : "all";

    // Validate stop count before parsing (only when transit tab is being saved)
    if (tab == "transit" || tab == "all")
    {
        if (server->hasArg("prague_stops"))
        {
            String stops = server->arg("prague_stops");
            int numStops = countStops(stops.c_str());
            if (numStops > 12)
            {
                server->send(400, "text/plain",
                    "Error: Too many stops configured (max 12). Please reduce the number of stops.\n"
                    "With 1-second delay between API calls, 12 stops takes 12+ seconds to query.");
                logTimestamp();
                debugPrintln("Config save failed: too many Prague stops");
                return;
            }
        }
        if (server->hasArg("berlin_stops"))
        {
            String stops = server->arg("berlin_stops");
            int numStops = countStops(stops.c_str());
            if (numStops > 12)
            {
                server->send(400, "text/plain",
                    "Error: Too many stops configured (max 12). Please reduce the number of stops.\n"
                    "With 1-second delay between API calls, 12 stops takes 12+ seconds to query.");
                logTimestamp();
                debugPrintln("Config save failed: too many Berlin stops");
                return;
            }
        }
    }

    // Parse configuration using tab-specific dispatch
    // Only parse fields belonging to the active tab to avoid:
    // 1. Wasting heap parsing fields that weren't sent
    // 2. Resetting checkboxes (absent = false) from other tabs
    if (tab == "connection" || tab == "all")
    {
        parseWifiSettings(&newConfig, &wifiChanged);
        parseConnectionSettings(&newConfig, &cityChanged);
    }
    if (tab == "transit" || tab == "all")
    {
        parseTransitSettings(&newConfig);
        parsePragueSettings(&newConfig);
        parseBerlinSettings(&newConfig);
        parseMqttSettings(&newConfig);
    }
    if (tab == "display" || tab == "all")
    {
        parseDisplaySettings(&newConfig);
        displaySizeChanged = (newConfig.panelRows != currentConfig->panelRows);
    }
    if (tab == "optional" || tab == "all")
    {
        parseOptionalSettings(&newConfig);
    }

    newConfig.configured = true;

    // If in AP mode, WiFi changed, city changed, or display size changed, show restart message
    if (apModeActive || wifiChanged || cityChanged || displaySizeChanged)
    {
        String html = FPSTR(HTML_HEADER);

        // Header
        html += "<div class='header'><div class='header-top'>";
        html += "<div class='header-title'><h1>SpojBoard</h1>";
        html += "<div class='header-subtitle'>Configuration Saved</div></div></div></div>";

        html += "<div class='content'>";

        // Main restart banner with loading animation
        html += "<div class='banner banner-info' style='margin-bottom:24px;'>";
        html += "<div class='status-dot' style='animation: pulse 1.5s ease-in-out infinite;'></div>";
        html += "<div style='flex:1;'><strong>Device is restarting...</strong></div>";
        html += "</div>";

        // Configuration change card
        html += "<div class='card'>";
        if (cityChanged)
        {
            html += "<h2 style='margin-top:0; color:#67e8f9; font-size:18px;'>🌍 Transit Provider Changed</h2>";
            html += "<p style='margin:12px 0; font-size:14px;'>New provider: <strong style='color:#2ed573;'>" + String(newConfig.city) + "</strong></p>";
            html += "<p style='color:#999; font-size:13px;'>The device needs to restart to initialize the new transit API configuration.</p>";
        }
        else
        {
            html += "<h2 style='margin-top:0; color:#67e8f9; font-size:18px;'>📡 WiFi Network Changed</h2>";
            html += "<p style='margin:12px 0; font-size:14px;'>Connecting to: <strong style='color:#2ed573;'>" + String(newConfig.wifiSsid) + "</strong></p>";
            html += "<p style='color:#999; font-size:13px;'>The device will restart and attempt to connect to the new network.</p>";
        }
        html += "</div>";

        // What to expect section
        html += "<div class='card' style='background:#0a0a0a; border:1px solid #333;'>";
        html += "<h3 style='margin-top:0; font-size:14px; color:#999; text-transform:uppercase; letter-spacing:0.5px;'>What to Expect</h3>";
        html += "<ul style='margin:8px 0; padding-left:20px; color:#999; font-size:13px; line-height:1.8;'>";
        html += "<li>Device restarts in <strong style='color:#f5f5f5;'>~3 seconds</strong></li>";
        html += "<li>Boot and connection takes <strong style='color:#f5f5f5;'>10-15 seconds</strong></li>";
        if (!cityChanged && !apModeActive) {
            html += "<li>If WiFi fails, device returns to <strong style='color:#fcd34d;'>AP mode</strong></li>";
        }
        html += "<li>Access dashboard at device's new IP address</li>";
        html += "</ul>";
        html += "</div>";

        // Progress bar
        html += "<div style='margin:24px 0;'>";
        html += "<div style='background:#1a1a1a; height:6px; border-radius:3px; overflow:hidden;'>";
        html += "<div id='progress-bar' style='background:linear-gradient(90deg, #67e8f9, #2ed573); height:100%; width:0%; transition:width 15s linear;'></div>";
        html += "</div>";
        html += "<div id='status-text' style='text-align:center; margin-top:8px; color:#999; font-size:12px;'>Restarting device...</div>";
        html += "</div>";

        // Reconnect button (hidden initially)
        html += "<div id='reconnect-section' style='display:none; margin-top:24px;'>";
        html += "<button onclick='window.location=\"/\"' class='btn-primary' style='background:#2ed573;'>✓ Reconnect to Device</button>";
        html += "<p style='text-align:center; margin-top:12px; color:#666; font-size:12px;'>Click to return to the dashboard</p>";
        html += "</div>";

        html += "</div>"; // End content

        // Animation and auto-reconnect script
        html += "<style>";
        html += "@keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.3; } }";
        html += "</style>";
        html += "<script>";
        html += "setTimeout(function(){ document.getElementById('progress-bar').style.width='100%'; }, 100);";
        html += "setTimeout(function(){ document.getElementById('status-text').textContent='Waiting for device...'; }, 5000);";
        html += "setTimeout(function(){ ";
        html += "  document.getElementById('reconnect-section').style.display='block';";
        html += "  document.getElementById('status-text').textContent='Device should be ready';";
        html += "}, 15000);";
        html += "</script>";

        html += FPSTR(HTML_FOOTER);
        server->send(200, "text/html", html);
    }
    else
    {
        // Normal save without WiFi change - send JSON response for AJAX handling
        server->send(200, "application/json", "{\"success\":true,\"message\":\"Configuration saved\"}");
    }

    // Call the callback to notify main.cpp
    // Pass true for restart if either WiFi or city changed
    onSaveCallback(newConfig, wifiChanged || cityChanged || displaySizeChanged, tab.c_str());
}

// ============================================================================
// Config Parsing Helper Methods
// ============================================================================

void ConfigWebServer::parseWifiSettings(Config* config, bool* wifiChanged)
{
    if (server->hasArg("ssid"))
    {
        String newSsid = server->arg("ssid");
        if (newSsid != config->wifiSsid)
        {
            *wifiChanged = true;
        }
        strlcpy(config->wifiSsid, newSsid.c_str(), sizeof(config->wifiSsid));
    }

    if (server->hasArg("password") && server->arg("password").length() > 0)
    {
        strlcpy(config->wifiPassword, server->arg("password").c_str(), sizeof(config->wifiPassword));
        *wifiChanged = true;
    }
}

void ConfigWebServer::parseConnectionSettings(Config* config, bool* cityChanged)
{
    if (server->hasArg("city"))
    {
        String newCity = server->arg("city");
        if (newCity == "Berlin" || newCity == "Prague" || newCity == "MQTT")
        {
            if (newCity != config->city)
            {
                *cityChanged = true;
            }
            strlcpy(config->city, newCity.c_str(), sizeof(config->city));
        }
        else
        {
            strlcpy(config->city, "Prague", sizeof(config->city));
        }
    }
}

void ConfigWebServer::parseTransitSettings(Config* config)
{
    if (server->hasArg("refresh"))
    {
        config->refreshInterval = server->arg("refresh").toInt();
        if (config->refreshInterval < 10)
            config->refreshInterval = 10;
        if (config->refreshInterval > 300)
            config->refreshInterval = 300;
    }

    if (server->hasArg("min_dep_time"))
    {
        config->minDepartureTime = server->arg("min_dep_time").toInt();
        if (config->minDepartureTime < 0)
            config->minDepartureTime = 0;
        if (config->minDepartureTime > 30)
            config->minDepartureTime = 30;
    }
}

void ConfigWebServer::parseDisplaySettings(Config* config)
{
    if (server->hasArg("panel_rows"))
    {
        int newPanelRows = server->arg("panel_rows").toInt();
        if (newPanelRows < 1) newPanelRows = 1;
        if (newPanelRows > 2) newPanelRows = 2;
        config->panelRows = newPanelRows;
    }

    if (server->hasArg("brightness"))
    {
        config->brightness = server->arg("brightness").toInt();
        if (config->brightness < 0)
            config->brightness = 0;
        if (config->brightness > 255)
            config->brightness = 255;
    }

    if (server->hasArg("num_deps"))
    {
        int maxDeps = (config->panelRows * 32 / 8) - 1;
        config->numDepartures = server->arg("num_deps").toInt();
        if (config->numDepartures < 1)
            config->numDepartures = 1;
        if (config->numDepartures > maxDeps)
            config->numDepartures = maxDeps;
    }

    if (server->hasArg("language"))
    {
        String lang = server->arg("language");
        if (lang == "cs" || lang == "de" || lang == "en")
        {
            strlcpy(config->language, lang.c_str(), sizeof(config->language));
        }
        else
        {
            strlcpy(config->language, "en", sizeof(config->language));
        }
    }

    // Checkboxes: safe to set here because this only runs when tab == "display"
    config->showPlatform = server->hasArg("show_platform");
    config->scrollEnabled = server->hasArg("scroll_enabled");
    config->showMultipleTimes = server->hasArg("show_multi_times");

    // Line color map (always update when not in AP mode to handle empty case)
    if (!apModeActive)
    {
        String colorMapValue = server->hasArg("line_color_map")
                               ? server->arg("line_color_map")
                               : "";
        strlcpy(config->lineColorMap, colorMapValue.c_str(), sizeof(config->lineColorMap));

        logTimestamp();
        Serial.print("Line color map updated: ");
        Serial.println(strlen(config->lineColorMap) > 0 ? config->lineColorMap : "(empty - using defaults)");
    }

    // Platform symbol map
    if (!apModeActive)
    {
        String symbolMapValue = server->hasArg("platform_symbol_map")
                                ? server->arg("platform_symbol_map")
                                : "";
        strlcpy(config->platformSymbolMap, symbolMapValue.c_str(), sizeof(config->platformSymbolMap));
    }
}

void ConfigWebServer::parseOptionalSettings(Config* config)
{
    // Debug mode checkbox
    config->debugMode = server->hasArg("debug_mode");

    // Weather settings (absorbed from old parseWeatherSettings)
    config->weatherEnabled = server->hasArg("weather_enabled");

    if (server->hasArg("weather_lat"))
    {
        String latStr = server->arg("weather_lat");
        latStr.replace(",", ".");
        config->weatherLatitude = latStr.toFloat();
        if (config->weatherLatitude < -90.0f)
            config->weatherLatitude = -90.0f;
        if (config->weatherLatitude > 90.0f)
            config->weatherLatitude = 90.0f;
    }

    if (server->hasArg("weather_lon"))
    {
        String lonStr = server->arg("weather_lon");
        lonStr.replace(",", ".");
        config->weatherLongitude = lonStr.toFloat();
        if (config->weatherLongitude < -180.0f)
            config->weatherLongitude = -180.0f;
        if (config->weatherLongitude > 180.0f)
            config->weatherLongitude = 180.0f;
    }

    if (server->hasArg("weather_refresh"))
    {
        config->weatherRefreshInterval = server->arg("weather_refresh").toInt();
        if (config->weatherRefreshInterval < 10)
            config->weatherRefreshInterval = 10;
        if (config->weatherRefreshInterval > 60)
            config->weatherRefreshInterval = 60;
    }

    // Rest mode periods
    if (server->hasArg("rest_periods"))
    {
        String restPeriods = server->arg("rest_periods");
        if (restPeriods.length() < sizeof(config->restModePeriods))
        {
            strlcpy(config->restModePeriods, restPeriods.c_str(), sizeof(config->restModePeriods));
        }
        else
        {
            logTimestamp();
            debugPrintln("RestMode: Config string too long, truncating");
            strlcpy(config->restModePeriods, restPeriods.c_str(), sizeof(config->restModePeriods));
        }
    }
}

void ConfigWebServer::parsePragueSettings(Config* config)
{
    String selectedCity = String(config->city);

    if (server->hasArg("api_key") && server->arg("api_key").length() > 0)
    {
        String apiKeyValue = server->arg("api_key");
        // Only save if it's not the placeholder dots (visual feedback, not actual key)
        if (apiKeyValue != "****" && selectedCity == "Prague")
        {
            strlcpy(config->pragueApiKey, apiKeyValue.c_str(), sizeof(config->pragueApiKey));
        }
    }

    if (server->hasArg("prague_stops") && selectedCity == "Prague")
    {
        String stops = server->arg("prague_stops");
        strlcpy(config->pragueStopIds, stops.c_str(), sizeof(config->pragueStopIds));
    }
}

void ConfigWebServer::parseBerlinSettings(Config* config)
{
    String selectedCity = String(config->city);

    if (server->hasArg("berlin_stops") && selectedCity == "Berlin")
    {
        String stops = server->arg("berlin_stops");
        strlcpy(config->berlinStopIds, stops.c_str(), sizeof(config->berlinStopIds));
    }
}

void ConfigWebServer::parseMqttSettings(Config* config)
{
    String selectedCity = String(config->city);

    // Only parse MQTT settings if city is MQTT
    if (selectedCity != "MQTT")
        return;

    if (server->hasArg("mqtt_broker"))
        strlcpy(config->mqttBroker, server->arg("mqtt_broker").c_str(), sizeof(config->mqttBroker));

    if (server->hasArg("mqtt_port"))
    {
        config->mqttPort = server->arg("mqtt_port").toInt();
        if (config->mqttPort < 1) config->mqttPort = 1;
        if (config->mqttPort > 65535) config->mqttPort = 65535;
    }

    if (server->hasArg("mqtt_user"))
        strlcpy(config->mqttUsername, server->arg("mqtt_user").c_str(), sizeof(config->mqttUsername));

    if (server->hasArg("mqtt_pass") && server->arg("mqtt_pass").length() > 0)
        strlcpy(config->mqttPassword, server->arg("mqtt_pass").c_str(), sizeof(config->mqttPassword));

    if (server->hasArg("mqtt_req_topic"))
        strlcpy(config->mqttRequestTopic, server->arg("mqtt_req_topic").c_str(), sizeof(config->mqttRequestTopic));

    if (server->hasArg("mqtt_resp_topic"))
        strlcpy(config->mqttResponseTopic, server->arg("mqtt_resp_topic").c_str(), sizeof(config->mqttResponseTopic));

    if (server->hasArg("mqtt_eta_mode"))
        config->mqttUseEtaMode = (server->arg("mqtt_eta_mode") == "1");

    // Parse JSON field mappings
    if (server->hasArg("mqtt_fld_line"))
        strlcpy(config->mqttFieldLine, server->arg("mqtt_fld_line").c_str(), sizeof(config->mqttFieldLine));

    if (server->hasArg("mqtt_fld_dest"))
        strlcpy(config->mqttFieldDestination, server->arg("mqtt_fld_dest").c_str(), sizeof(config->mqttFieldDestination));

    if (server->hasArg("mqtt_fld_eta"))
        strlcpy(config->mqttFieldEta, server->arg("mqtt_fld_eta").c_str(), sizeof(config->mqttFieldEta));

    if (server->hasArg("mqtt_fld_time"))
        strlcpy(config->mqttFieldTimestamp, server->arg("mqtt_fld_time").c_str(), sizeof(config->mqttFieldTimestamp));

    if (server->hasArg("mqtt_fld_plat"))
        strlcpy(config->mqttFieldPlatform, server->arg("mqtt_fld_plat").c_str(), sizeof(config->mqttFieldPlatform));

    if (server->hasArg("mqtt_fld_ac"))
        strlcpy(config->mqttFieldAC, server->arg("mqtt_fld_ac").c_str(), sizeof(config->mqttFieldAC));
}


void ConfigWebServer::handleRefresh()
{
    if (onRefreshCallback != nullptr)
    {
        onRefreshCallback();
    }

    server->sendHeader("Location", "/");
    server->send(302, "text/plain", "");
}

void ConfigWebServer::handleReboot()
{
    String html = FPSTR(HTML_HEADER);

    // Header
    html += "<div class='header'><div class='header-top'>";
    html += "<div class='header-title'><h1>SpojBoard</h1>";
    html += "<div class='header-subtitle'>Manual Reboot</div></div></div></div>";

    html += "<div class='content'>";

    // Main reboot banner with loading animation
    html += "<div class='banner banner-info' style='margin-bottom:24px;'>";
    html += "<div class='status-dot' style='animation: pulse 1.5s ease-in-out infinite;'></div>";
    html += "<div style='flex:1;'><strong>Device is rebooting...</strong></div>";
    html += "</div>";

    // Reboot reason card
    html += "<div class='card'>";
    html += "<h2 style='margin-top:0; color:#67e8f9; font-size:18px;'>🔄 System Reboot</h2>";
    html += "<p style='color:#999; font-size:13px;'>A manual reboot has been initiated. The device will restart and reload all configurations.</p>";
    html += "</div>";

    // What to expect section
    html += "<div class='card' style='background:#0a0a0a; border:1px solid #333;'>";
    html += "<h3 style='margin-top:0; font-size:14px; color:#999; text-transform:uppercase; letter-spacing:0.5px;'>What to Expect</h3>";
    html += "<ul style='margin:8px 0; padding-left:20px; color:#999; font-size:13px; line-height:1.8;'>";
    html += "<li>Device reboots in <strong style='color:#f5f5f5;'>~3 seconds</strong></li>";
    html += "<li>Boot process takes <strong style='color:#f5f5f5;'>10-15 seconds</strong></li>";
    html += "<li>Configuration and settings are preserved</li>";
    html += "<li>Device reconnects to WiFi automatically</li>";
    html += "</ul>";
    html += "</div>";

    // Progress bar
    html += "<div style='margin:24px 0;'>";
    html += "<div style='background:#1a1a1a; height:6px; border-radius:3px; overflow:hidden;'>";
    html += "<div id='progress-bar' style='background:linear-gradient(90deg, #67e8f9, #2ed573); height:100%; width:0%; transition:width 15s linear;'></div>";
    html += "</div>";
    html += "<div id='status-text' style='text-align:center; margin-top:8px; color:#999; font-size:12px;'>Rebooting device...</div>";
    html += "</div>";

    // Reconnect button (hidden initially)
    html += "<div id='reconnect-section' style='display:none; margin-top:24px;'>";
    html += "<button onclick='window.location=\"/\"' class='btn-primary' style='background:#2ed573;'>✓ Reconnect to Device</button>";
    html += "<p style='text-align:center; margin-top:12px; color:#666; font-size:12px;'>Click to return to the dashboard</p>";
    html += "</div>";

    html += "</div>"; // End content

    // Animation and auto-reconnect script
    html += "<style>";
    html += "@keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.3; } }";
    html += "</style>";
    html += "<script>";
    html += "setTimeout(function(){ document.getElementById('progress-bar').style.width='100%'; }, 100);";
    html += "setTimeout(function(){ document.getElementById('status-text').textContent='Waiting for device...'; }, 5000);";
    html += "setTimeout(function(){ ";
    html += "  document.getElementById('reconnect-section').style.display='block';";
    html += "  document.getElementById('status-text').textContent='Device should be ready';";
    html += "}, 15000);";
    html += "</script>";

    html += FPSTR(HTML_FOOTER);
    server->send(200, "text/html", html);

    if (onRebootCallback != nullptr)
    {
        onRebootCallback();
    }
}

void ConfigWebServer::handleClearConfig()
{
    String html = FPSTR(HTML_HEADER);

    // Header
    html += "<div class='header'><div class='header-top'>";
    html += "<div class='header-title'><h1>SpojBoard</h1>";
    html += "<div class='header-subtitle'>Factory Reset</div></div></div></div>";

    html += "<div class='content'>";

    // Warning banner
    html += "<div class='banner banner-error' style='margin-bottom:24px;'>";
    html += "<div class='status-dot' style='animation: pulse 1.5s ease-in-out infinite;'></div>";
    html += "<div style='flex:1;'><strong>Erasing all settings...</strong></div>";
    html += "</div>";

    // Reset details card
    html += "<div class='card' style='border:2px solid #fb7185;'>";
    html += "<h2 style='margin-top:0; color:#fb7185; font-size:18px;'>⚠️ Factory Reset in Progress</h2>";
    html += "<p style='margin:12px 0; font-size:14px; color:#f5f5f5;'>All configuration data has been <strong style='color:#fb7185;'>permanently erased</strong> from flash memory.</p>";
    html += "<p style='color:#999; font-size:13px;'>The device will reboot into AP (setup) mode. You'll need to reconfigure everything from scratch.</p>";
    html += "</div>";

    // What was cleared
    html += "<div class='card' style='background:#0a0a0a; border:1px solid #333;'>";
    html += "<h3 style='margin-top:0; font-size:14px; color:#999; text-transform:uppercase; letter-spacing:0.5px;'>Settings Cleared</h3>";
    html += "<ul style='margin:8px 0; padding-left:20px; color:#999; font-size:13px; line-height:1.8;'>";
    html += "<li><strong style='color:#fb7185;'>WiFi credentials</strong> (network name and password)</li>";
    html += "<li><strong style='color:#fb7185;'>Transit provider settings</strong> (API keys, stop IDs)</li>";
    html += "<li><strong style='color:#fb7185;'>Display preferences</strong> (brightness, colors, language)</li>";
    html += "<li><strong style='color:#fb7185;'>All custom configurations</strong></li>";
    html += "</ul>";
    html += "</div>";

    // What to expect
    html += "<div class='card' style='background:#0a0a0a; border:1px solid #333;'>";
    html += "<h3 style='margin-top:0; font-size:14px; color:#999; text-transform:uppercase; letter-spacing:0.5px;'>What to Expect</h3>";
    html += "<ul style='margin:8px 0; padding-left:20px; color:#999; font-size:13px; line-height:1.8;'>";
    html += "<li>Device reboots in <strong style='color:#f5f5f5;'>~10 seconds</strong></li>";
    html += "<li>Boots into <strong style='color:#fcd34d;'>AP (setup) mode</strong></li>";
    html += "<li>Creates WiFi hotspot: <strong style='color:#67e8f9;'>SpojBoard-XXXX</strong></li>";
    html += "<li>Connect to hotspot and configure from scratch</li>";
    html += "</ul>";
    html += "</div>";

    // Progress bar
    html += "<div style='margin:24px 0;'>";
    html += "<div style='background:#1a1a1a; height:6px; border-radius:3px; overflow:hidden;'>";
    html += "<div id='progress-bar' style='background:linear-gradient(90deg, #fb7185, #fcd34d); height:100%; width:0%; transition:width 15s linear;'></div>";
    html += "</div>";
    html += "<div id='status-text' style='text-align:center; margin-top:8px; color:#999; font-size:12px;'>Erasing configuration...</div>";
    html += "</div>";

    // AP mode instructions (shown after delay)
    html += "<div id='ap-instructions' style='display:none; margin-top:24px;'>";
    html += "<div class='banner banner-warning'>";
    html += "<div style='flex:1;'>";
    html += "<strong>Device is now in AP mode</strong><br>";
    html += "<span style='font-size:12px; opacity:0.8;'>Look for WiFi network: <strong>SpojBoard-XXXX</strong></span>";
    html += "</div></div>";
    html += "<p style='text-align:center; margin-top:12px; color:#666; font-size:12px;'>Connect to the hotspot to begin setup</p>";
    html += "</div>";

    html += "</div>"; // End content

    // Animation script
    html += "<style>";
    html += "@keyframes pulse { 0%, 100% { opacity: 1; } 50% { opacity: 0.3; } }";
    html += "</style>";
    html += "<script>";
    html += "setTimeout(function(){ document.getElementById('progress-bar').style.width='100%'; }, 100);";
    html += "setTimeout(function(){ document.getElementById('status-text').textContent='Rebooting into AP mode...'; }, 8000);";
    html += "setTimeout(function(){ ";
    html += "  document.getElementById('ap-instructions').style.display='block';";
    html += "  document.getElementById('status-text').textContent='Setup mode active';";
    html += "}, 15000);";
    html += "</script>";

    html += FPSTR(HTML_FOOTER);
    server->send(200, "text/html", html);

    // Clear all config from NVS
    clearConfig();

    // Reboot after a short delay
    delay(10000);
    ESP.restart();
}

void ConfigWebServer::handleUpdate()
{
    // Block OTA upload in AP mode (security measure)
    if (apModeActive)
    {
        server->send(403, "text/html", buildUpdateBlockedPage());
        return;
    }

    server->send(200, "text/html", buildUpdatePage());
}

void ConfigWebServer::otaProgressCallback(size_t progress, size_t total)
{
    // Static callback that forwards to instance method
    if (instanceForCallback != nullptr && instanceForCallback->displayManager != nullptr)
    {
        instanceForCallback->displayManager->drawOTAProgress(progress, total);
    }
}

void ConfigWebServer::handleUpdateProgress()
{
    // This function is called during upload to process chunks
    // It should NOT send any HTTP responses

    if (otaManager == nullptr)
    {
        return;
    }

    // Block uploads in AP mode
    if (apModeActive)
    {
        return;
    }

    // Let OTA manager handle the upload with progress callback
    otaManager->handleUpload(server, otaProgressCallback);
}

void ConfigWebServer::handleUpdateComplete()
{
    // This function is called once after upload completes
    // It sends the final HTTP response

    if (otaManager == nullptr)
    {
        server->send(500, "text/plain", "OTA manager not initialized");
        return;
    }

    // Block uploads in AP mode
    if (apModeActive)
    {
        server->send(403, "text/plain", "OTA updates disabled in AP mode");
        return;
    }

    // Check if upload succeeded or failed
    if (strlen(otaManager->getError()) > 0)
    {
        // Error occurred
        server->send(500, "text/html", buildUpdateErrorPage(otaManager->getError()));
    }
    else
    {
        // Success
        server->send(200, "text/html", buildUpdateSuccessPage());

        // Reboot after a short delay
        delay(10000);
        ESP.restart();
    }
}

void ConfigWebServer::handleNotFound()
{
    // Captive portal redirect - redirect all unknown requests to root
    if (apModeActive)
    {
        server->sendHeader("Location", "http://192.168.4.1/");
        server->send(302, "text/plain", "");
    }
    else
    {
        server->sendHeader("Location", "/");
        server->send(302, "text/plain", "");
    }
}

void ConfigWebServer::githubOtaProgressCallback(size_t progress, size_t total)
{
    // Static callback that forwards to instance method
    if (instanceForCallback != nullptr &&
        instanceForCallback->displayManager != nullptr)
    {
        instanceForCallback->displayManager->drawOTAProgress(progress, total);
    }
}

void ConfigWebServer::handleCheckUpdate()
{
    // Block if in AP mode
    if (apModeActive)
    {
        server->send(403, "application/json", "{\"error\":\"Updates not available in AP mode\"}");
        return;
    }

    logTimestamp();
    Serial.println("Checking for GitHub updates...");

    // Check for updates
    GitHubOTA::ReleaseInfo info = githubOTA->checkForUpdate(FIRMWARE_RELEASE);

    // Build JSON response using ApiHandlers
    String json = buildCheckUpdateJson(info);

    server->send(200, "application/json", json);
}

void ConfigWebServer::handleDownloadUpdate()
{
    // Block if in AP mode
    if (apModeActive)
    {
        server->send(403, "application/json", "{\"success\":false,\"error\":\"Updates not available in AP mode\"}");
        return;
    }

    // Parse JSON request body
    String body = server->arg("plain");

    // Simple JSON parsing (extract assetUrl and expectedSize)
    int urlStart = body.indexOf("\"assetUrl\":\"") + 12;
    int urlEnd = body.indexOf("\"", urlStart);
    String assetUrl = body.substring(urlStart, urlEnd);

    int sizeStart = body.indexOf("\"expectedSize\":") + 15;
    int sizeEnd = body.indexOf(",", sizeStart);
    if (sizeEnd < 0)
        sizeEnd = body.indexOf("}", sizeStart);
    String sizeStr = body.substring(sizeStart, sizeEnd);
    size_t expectedSize = sizeStr.toInt();

    if (assetUrl.length() == 0 || expectedSize == 0)
    {
        server->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid request parameters\"}");
        return;
    }

    logTimestamp();
    Serial.print("Downloading update from: ");
    Serial.println(assetUrl);

    // Download and install
    bool success = githubOTA->downloadAndInstall(assetUrl.c_str(), expectedSize, githubOtaProgressCallback);

    if (success)
    {
        server->send(200, "application/json", "{\"success\":true,\"message\":\"Rebooting...\"}");

        logTimestamp();
        Serial.println("Update successful, rebooting in 10 seconds...");

        delay(10000);
        ESP.restart();
    }
    else
    {
        server->send(500, "application/json", "{\"success\":false,\"error\":\"Download or installation failed\"}");
    }
}

void ConfigWebServer::handleDemo()
{
    server->send(200, "text/html", buildDemoPage());
}

void ConfigWebServer::handleStartDemo()
{
    // Parse JSON request body
    String body = server->arg("plain");

    // Simple JSON parsing for departures array
    Departure demoDepartures[3];
    int demoCount = 0;

    // Extract departure data from JSON (manual parsing for simplicity)
    for (int i = 0; i < 3; i++)
    {
        // Find "line":"<value>" pattern
        String lineKey = "\"line\":\"";
        int lineStart = body.indexOf(lineKey, 0);
        if (lineStart < 0) break;
        lineStart += lineKey.length();
        int lineEnd = body.indexOf("\"", lineStart);
        String lineValue = body.substring(lineStart, lineEnd);

        // Find "destination":"<value>" pattern
        String destKey = "\"destination\":\"";
        int destStart = body.indexOf(destKey, lineEnd);
        if (destStart < 0) break;
        destStart += destKey.length();
        int destEnd = body.indexOf("\"", destStart);
        String destValue = body.substring(destStart, destEnd);

        // Find "eta":<value> pattern
        String etaKey = "\"eta\":";
        int etaStart = body.indexOf(etaKey, destEnd);
        if (etaStart < 0) break;
        etaStart += etaKey.length();
        int etaEnd = body.indexOf(",", etaStart);
        if (etaEnd < 0) etaEnd = body.indexOf("}", etaStart);
        String etaValue = body.substring(etaStart, etaEnd);

        // Find "platform":"<value>" pattern (optional)
        String platformKey = "\"platform\":\"";
        int platformStart = body.indexOf(platformKey, etaEnd);
        String platformValue = "";
        int lastFieldEnd = etaEnd;
        if (platformStart >= 0)
        {
            platformStart += platformKey.length();
            int platformEnd = body.indexOf("\"", platformStart);
            platformValue = body.substring(platformStart, platformEnd);
            lastFieldEnd = platformEnd;
        }

        // Find "hasAC":<value> pattern
        String acKey = "\"hasAC\":";
        int acStart = body.indexOf(acKey, lastFieldEnd);
        bool hasAC = false;
        if (acStart >= 0)
        {
            acStart += acKey.length();
            int acEnd = body.indexOf(",", acStart);
            if (acEnd < 0) acEnd = body.indexOf("}", acStart);
            String acValue = body.substring(acStart, acEnd);
            hasAC = acValue.indexOf("true") >= 0;
        }

        // Find "secondEta":<value> pattern (optional, -1 = none)
        int secondEta = -1;
        String secondEtaKey = "\"secondEta\":";
        int secondEtaStart = body.indexOf(secondEtaKey, lastFieldEnd);
        if (secondEtaStart >= 0)
        {
            secondEtaStart += secondEtaKey.length();
            int secondEtaEnd = body.indexOf(",", secondEtaStart);
            if (secondEtaEnd < 0) secondEtaEnd = body.indexOf("}", secondEtaStart);
            String secondEtaValue = body.substring(secondEtaStart, secondEtaEnd);
            secondEta = secondEtaValue.toInt();
            // toInt returns 0 for non-numeric; distinguish from actual 0
            if (secondEta == 0 && secondEtaValue.indexOf("-1") >= 0)
                secondEta = -1;
        }

        // Copy to departure structure
        strlcpy(demoDepartures[demoCount].line, lineValue.c_str(), sizeof(demoDepartures[demoCount].line));
        strlcpy(demoDepartures[demoCount].destination, destValue.c_str(), sizeof(demoDepartures[demoCount].destination));
        demoDepartures[demoCount].eta = etaValue.toInt();
        strlcpy(demoDepartures[demoCount].platform, platformValue.c_str(), sizeof(demoDepartures[demoCount].platform));
        demoDepartures[demoCount].hasAC = hasAC;
        demoDepartures[demoCount].isDelayed = false;
        demoDepartures[demoCount].delayMinutes = 0;
        demoDepartures[demoCount].departureTime = 0;
        demoDepartures[demoCount].secondEta = secondEta;
        demoDepartures[demoCount].secondDepartureTime = 0;

        demoCount++;

        // Move search position forward for next departure
        body = body.substring(etaEnd + 1);
    }

    if (demoCount == 0)
    {
        server->send(400, "application/json", "{\"success\":false,\"error\":\"No departure data found\"}");
        return;
    }

    // Call callback to activate demo mode
    if (onDemoStartCallback != nullptr)
    {
        onDemoStartCallback(demoDepartures, demoCount);
    }

    // Show demo on display immediately
    if (displayManager != nullptr)
    {
        displayManager->drawDemo(demoDepartures, demoCount, "Demo Mode");
    }

    logTimestamp();
    Serial.print("Demo mode started with ");
    Serial.print(demoCount);
    Serial.println(" departures");

    server->send(200, "application/json", "{\"success\":true,\"message\":\"Demo mode activated\"}");
}

void ConfigWebServer::handleStopDemo()
{
    // Call callback to deactivate demo mode
    if (onDemoStopCallback != nullptr)
    {
        onDemoStopCallback();
    }

    logTimestamp();
    Serial.println("Demo mode stopped");

    server->sendHeader("Location", "/");
    server->send(302, "text/plain", "");
}

void ConfigWebServer::handleInfoText()
{
    server->send(200, "text/html", buildInfoTextPage());
}

void ConfigWebServer::handleSetInfoText()
{
    String body = server->arg("plain");

    // Infotext is at most 512 bytes (MAX_INFOTEXT_LEN); 768 covers the value
    // copy plus object/key overhead with headroom.
    StaticJsonDocument<768> doc;
    DeserializationError error = deserializeJson(doc, body);
    if (error)
    {
        server->send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        return;
    }

    // Null when "text" is absent or not a string
    const char* text = doc["text"];
    if (text == nullptr)
    {
        server->send(400, "application/json", "{\"success\":false,\"error\":\"Missing text field\"}");
        return;
    }

    if (displayManager != nullptr)
    {
        displayManager->setInfoTextManual(text);
    }

    logTimestamp();
    Serial.print("Infotext set: ");
    Serial.println(text);

    server->send(200, "application/json", "{\"success\":true}");
}

void ConfigWebServer::handleClearInfoText()
{
    if (displayManager != nullptr)
    {
        displayManager->clearInfoText();
    }

    logTimestamp();
    Serial.println("Infotext cleared");

    server->send(200, "application/json", "{\"success\":true}");
}

void ConfigWebServer::handleCurrentInfoText()
{
    bool active = (displayManager != nullptr && displayManager->isInfoTextActive());

    StaticJsonDocument<768> doc;
    doc["active"] = active;
    doc["manual"] = active && displayManager->isInfoTextManual();
    doc["text"] = active ? displayManager->getInfoTextRaw() : "";

    // serializeJson escapes the full JSON character set (quotes, backslashes,
    // control chars); String output grows as needed, so no truncation.
    String response;
    serializeJson(doc, response);
    server->send(200, "application/json", response);
}

void ConfigWebServer::handleRestMode()
{
    // Block in AP mode
    if (apModeActive)
    {
        server->send(403, "application/json", "{\"success\":false,\"error\":\"Rest mode control not available in AP mode\"}");
        return;
    }

    // Parse JSON request body: {"enabled": true/false}
    String body = server->arg("plain");

    // Simple JSON parsing for "enabled" field
    bool enabled = false;
    int enabledStart = body.indexOf("\"enabled\":");
    if (enabledStart >= 0)
    {
        enabledStart += 10; // Length of "\"enabled\":"
        // Skip whitespace
        while (enabledStart < (int)body.length() && (body[enabledStart] == ' ' || body[enabledStart] == '\t'))
        {
            enabledStart++;
        }
        enabled = body.substring(enabledStart, enabledStart + 4) == "true";
    }
    else
    {
        server->send(400, "application/json", "{\"success\":false,\"error\":\"Missing 'enabled' field\"}");
        return;
    }

    // Call callback to control rest mode
    if (onRestModeCallback != nullptr)
    {
        onRestModeCallback(enabled);
    }

    logTimestamp();
    Serial.print("Rest mode ");
    Serial.println(enabled ? "enabled via REST API" : "disabled via REST API");

    String response = "{\"success\":true,\"restMode\":";
    response += enabled ? "true" : "false";
    response += "}";
    server->send(200, "application/json", response);
}

// ============================================================================
// Ticker Mode Handlers
// ============================================================================

void ConfigWebServer::handleTicker()
{
    if (currentConfig == nullptr)
    {
        server->send(500, "text/plain", "Server not initialized");
        return;
    }

    server->send(200, "text/html", buildTickerPage(
        tickerModeActive,
        currentConfig->tickerSymbol,
        currentConfig->tickerInterval,
        currentConfig->tickerRefreshInterval,
        currentConfig->tickerApiKey[0] != '\0'
    ));
}

void ConfigWebServer::handleStartTicker()
{
    if (currentConfig == nullptr)
    {
        server->send(500, "application/json", "{\"success\":false,\"error\":\"Server not initialized\"}");
        return;
    }

    // Parse form data and save to config
    Config newConfig = *currentConfig;

    if (server->hasArg("ticker_symbol"))
    {
        strlcpy(newConfig.tickerSymbol, server->arg("ticker_symbol").c_str(), sizeof(newConfig.tickerSymbol));
    }

    if (server->hasArg("ticker_interval"))
    {
        String interval = server->arg("ticker_interval");
        if (interval == "1h" || interval == "4h" || interval == "1day")
        {
            strlcpy(newConfig.tickerInterval, interval.c_str(), sizeof(newConfig.tickerInterval));
        }
    }

    if (server->hasArg("ticker_api_key") && server->arg("ticker_api_key").length() > 0)
    {
        strlcpy(newConfig.tickerApiKey, server->arg("ticker_api_key").c_str(), sizeof(newConfig.tickerApiKey));
    }

    if (server->hasArg("ticker_refresh"))
    {
        newConfig.tickerRefreshInterval = server->arg("ticker_refresh").toInt();
        if (newConfig.tickerRefreshInterval < 120) newConfig.tickerRefreshInterval = 120;
        if (newConfig.tickerRefreshInterval > 600) newConfig.tickerRefreshInterval = 600;
    }

    // Validate API key is present
    if (newConfig.tickerApiKey[0] == '\0')
    {
        server->send(400, "application/json", "{\"success\":false,\"error\":\"API key is required\"}");
        return;
    }

    // Enable and persist
    newConfig.tickerEnabled = true;
    newConfig.configured = true;

    // Update global config
    // Note: onSaveCallback would restart if WiFi changed, so we save directly here
    *const_cast<Config*>(currentConfig) = newConfig;
    saveConfig(newConfig);

    // Activate ticker via callback
    if (onTickerStartCallback != nullptr)
    {
        onTickerStartCallback();
    }

    logTimestamp();
    Serial.print("Ticker started: ");
    Serial.println(newConfig.tickerSymbol);

    server->send(200, "application/json", "{\"success\":true,\"message\":\"Ticker mode activated\"}");
}

void ConfigWebServer::handleStopTicker()
{
    if (onTickerStopCallback != nullptr)
    {
        onTickerStopCallback();
    }

    logTimestamp();
    Serial.println("Ticker mode stopped");

    server->sendHeader("Location", "/");
    server->send(302, "text/plain", "");
}

void ConfigWebServer::handleTickerMode()
{
    // Block in AP mode
    if (apModeActive)
    {
        server->send(403, "application/json", "{\"success\":false,\"error\":\"Ticker mode not available in AP mode\"}");
        return;
    }

    // Parse JSON request body: {"enabled": true/false}
    String body = server->arg("plain");

    bool enabled = false;
    int enabledStart = body.indexOf("\"enabled\":");
    if (enabledStart >= 0)
    {
        enabledStart += 10;
        while (enabledStart < (int)body.length() && (body[enabledStart] == ' ' || body[enabledStart] == '\t'))
        {
            enabledStart++;
        }
        enabled = body.substring(enabledStart, enabledStart + 4) == "true";
    }
    else
    {
        server->send(400, "application/json", "{\"success\":false,\"error\":\"Missing 'enabled' field\"}");
        return;
    }

    if (onTickerModeCallback != nullptr)
    {
        onTickerModeCallback(enabled);
    }

    logTimestamp();
    Serial.print("Ticker mode ");
    Serial.println(enabled ? "enabled via REST API" : "disabled via REST API");

    String response = "{\"success\":true,\"tickerMode\":";
    response += enabled ? "true" : "false";
    response += "}";
    server->send(200, "application/json", response);
}

// ============================================================================
// Departure Data Viewer
// ============================================================================

// Access global departure cache from main.cpp (protected by apiDataMutex)
extern Departure departures[];
extern int departureCount;
extern SemaphoreHandle_t apiDataMutex;

void ConfigWebServer::handleDepartures()
{
    // Acquire mutex to safely snapshot departure data
    Departure localDeps[MAX_DEPARTURES];
    int localCount = 0;

    if (apiDataMutex != NULL && xSemaphoreTake(apiDataMutex, pdMS_TO_TICKS(100)))
    {
        localCount = departureCount;
        for (int i = 0; i < localCount && i < MAX_DEPARTURES; i++)
        {
            localDeps[i] = departures[i];
        }
        xSemaphoreGive(apiDataMutex);
    }

    server->send(200, "text/html", buildDeparturesPage(localDeps, localCount));
}

void ConfigWebServer::handleDeparturesData()
{
    // Acquire mutex to safely snapshot departure data
    Departure localDeps[MAX_DEPARTURES];
    int localCount = 0;

    if (apiDataMutex != NULL && xSemaphoreTake(apiDataMutex, pdMS_TO_TICKS(100)))
    {
        localCount = departureCount;
        for (int i = 0; i < localCount && i < MAX_DEPARTURES; i++)
        {
            localDeps[i] = departures[i];
        }
        xSemaphoreGive(apiDataMutex);
    }

    server->send(200, "application/json", buildDeparturesJson(localDeps, localCount));
}
