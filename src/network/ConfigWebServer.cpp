#include "ConfigWebServer.h"
#include "../utils/Logger.h"
#include "../display/DisplayManager.h"
#include "../api/WeatherAPI.h"
#include "web/WebTemplates.h"
#include "web/DashboardPage.h"
#include "web/DemoPage.h"
#include "web/UpdatePage.h"
#include "web/PreviewPage.h"
#include "web/ApiHandlers.h"
#include "web/WebUtils.h"
#include <WiFi.h>
#include <Update.h>
#include <string.h>

// Static instance pointer for OTA callback
ConfigWebServer *ConfigWebServer::instanceForCallback = nullptr;

ConfigWebServer::ConfigWebServer()
    : server(nullptr), otaManager(nullptr), githubOTA(nullptr), displayManager(nullptr),
      currentConfig(nullptr),
      wifiConnected(false), apModeActive(false),
      apSSID(""), apPassword(""), apClientCount(0),
      apiError(false), apiErrorMsg(""), departureCount(0), stopName(""),
      demoModeActive(false), restModeActive(false), restModeManual(false),
      onSaveCallback(nullptr), onRefreshCallback(nullptr), onRebootCallback(nullptr),
      onDemoStartCallback(nullptr), onDemoStopCallback(nullptr), onRestModeCallback(nullptr)
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
    server->on("/api/display-state", HTTP_GET, [this]()
               { handleDisplayStateAPI(); });
    server->on("/preview", HTTP_GET, [this]()
               { handlePreviewPage(); });
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
                                  RestModeCallback onRestMode)
{
    onSaveCallback = onSave;
    onRefreshCallback = onRefresh;
    onRebootCallback = onReboot;
    onDemoStartCallback = onDemoStart;
    onDemoStopCallback = onDemoStop;
    onRestModeCallback = onRestMode;
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
                                  bool demoMode, bool restMode, bool restManual)
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
}

void ConfigWebServer::handleRoot()
{
    if (currentConfig == nullptr)
    {
        server->send(500, "text/plain", "Server not initialized");
        return;
    }

    String html = buildDashboardPage(
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

    server->send(200, "text/html", html);
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

    // Validate stop count before parsing
    if (server->hasArg("stops"))
    {
        String stops = server->arg("stops");
        int numStops = countStops(stops.c_str());
        if (numStops > 12)
        {
            server->send(400, "text/plain",
                "Error: Too many stops configured (max 12). Please reduce the number of stops.\n"
                "With 1-second delay between API calls, 12 stops takes 12+ seconds to query.");
            logTimestamp();
            debugPrintln("Config save failed: too many stops");
            return;
        }
    }

    // Parse configuration using helper methods
    parseWifiSettings(&newConfig, &wifiChanged);
    parseGeneralSettings(&newConfig, &cityChanged);
    parsePragueSettings(&newConfig);
    parseBerlinSettings(&newConfig);
    parseMqttSettings(&newConfig);
    parseWeatherSettings(&newConfig);

    newConfig.configured = true;

    // If in AP mode, WiFi changed, or city changed, show restart message
    if (apModeActive || wifiChanged || cityChanged)
    {
        String html = FPSTR(HTML_HEADER);
        html += "<h1>Restarting...</h1>";
        if (cityChanged)
        {
            html += "<p>Transit city changed to: <strong>" + String(newConfig.city) + "</strong></p>";
            html += "<p>The device will restart to apply the new transit API configuration.</p>";
            html += "<p>Please wait 10-15 seconds for it to come back online.</p>";
        }
        else
        {
            html += "<p>Attempting to connect to WiFi network: <strong>" + String(newConfig.wifiSsid) + "</strong></p>";
            html += "<p>Please wait... The device will restart and connect to the new network.</p>";
            html += "<p>If connection fails, the device will return to AP mode.</p>";
        }
        html += "<div class='card'>";
        html += "<p>After successful restart, access the device at its IP address.</p>";
        html += "</div>";
        html += "<div id='reconnect-msg' style='display:none; margin-top:20px;'>";
        html += "<p><button onclick='window.location=\"/\"' style='padding:12px 24px; font-size:16px; cursor:pointer; background:#2ed573; color:#000; border:none; border-radius:8px;'>Reconnect to Device</button></p>";
        html += "</div>";
        html += "<script>setTimeout(function(){ document.getElementById('reconnect-msg').style.display='block'; }, 10000);</script>";
        html += FPSTR(HTML_FOOTER);
        server->send(200, "text/html", html);
    }
    else
    {
        // Normal save without WiFi change
        String html = FPSTR(HTML_HEADER);
        html += "<h1>Configuration Saved</h1>";
        html += "<p>Settings have been saved. The device will apply them immediately.</p>";
        html += "<p><a href='/'>Back to Dashboard</a></p>";
        html += FPSTR(HTML_FOOTER);
        server->send(200, "text/html", html);
    }

    // Call the callback to notify main.cpp
    // Pass true for restart if either WiFi or city changed
    onSaveCallback(newConfig, wifiChanged || cityChanged);
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

void ConfigWebServer::parseGeneralSettings(Config* config, bool* cityChanged)
{
    // Parse city field
    if (server->hasArg("city"))
    {
        String newCity = server->arg("city");
        // Validate city value
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
            // Invalid city value, default to Prague
            strlcpy(config->city, "Prague", sizeof(config->city));
        }
    }

    // Refresh interval
    if (server->hasArg("refresh"))
    {
        config->refreshInterval = server->arg("refresh").toInt();
        if (config->refreshInterval < 10)
            config->refreshInterval = 10;
        if (config->refreshInterval > 300)
            config->refreshInterval = 300;
    }

    // Number of departures
    if (server->hasArg("numdeps"))
    {
        config->numDepartures = server->arg("numdeps").toInt();
        if (config->numDepartures < 1)
            config->numDepartures = 1;
        if (config->numDepartures > 3)
            config->numDepartures = 3;  // Max 3 rows on display
    }

    // Minimum departure time
    if (server->hasArg("mindeptime"))
    {
        config->minDepartureTime = server->arg("mindeptime").toInt();
        if (config->minDepartureTime < 0)
            config->minDepartureTime = 0;
        if (config->minDepartureTime > 30)
            config->minDepartureTime = 30;
    }

    // Brightness
    if (server->hasArg("brightness"))
    {
        config->brightness = server->arg("brightness").toInt();
        if (config->brightness < 0)
            config->brightness = 0;
        if (config->brightness > 255)
            config->brightness = 255;
    }

    // Language
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

    // Debug mode checkbox (unchecked = not present in POST data)
    config->debugMode = server->hasArg("debugmode");

    // Show platform checkbox (unchecked = not present in POST data)
    config->showPlatform = server->hasArg("showplatform");

    // Scrolling checkbox (unchecked = not present in POST data)
    config->scrollEnabled = server->hasArg("scrollenabled");

    // Line color map (always update when not in AP mode to handle empty case)
    if (!apModeActive)
    {
        // Get the value, defaulting to empty string if not present
        String colorMapValue = server->hasArg("linecolormap")
                               ? server->arg("linecolormap")
                               : "";

        strlcpy(config->lineColorMap, colorMapValue.c_str(), sizeof(config->lineColorMap));

        // Log configuration
        logTimestamp();
        Serial.print("Line color map updated: ");
        Serial.println(strlen(config->lineColorMap) > 0 ? config->lineColorMap : "(empty - using defaults)");
    }

    // Rest mode periods
    if (server->hasArg("restperiods"))
    {
        String restPeriods = server->arg("restperiods");

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

    if (server->hasArg("apikey") && server->arg("apikey").length() > 0)
    {
        String apiKeyValue = server->arg("apikey");
        // Only save if it's not the placeholder dots (visual feedback, not actual key)
        if (apiKeyValue != "****" && selectedCity == "Prague")
        {
            strlcpy(config->pragueApiKey, apiKeyValue.c_str(), sizeof(config->pragueApiKey));
        }
    }

    if (server->hasArg("stops") && selectedCity == "Prague")
    {
        String stops = server->arg("stops");
        strlcpy(config->pragueStopIds, stops.c_str(), sizeof(config->pragueStopIds));
    }
}

void ConfigWebServer::parseBerlinSettings(Config* config)
{
    String selectedCity = String(config->city);

    if (server->hasArg("stops") && selectedCity == "Berlin")
    {
        String stops = server->arg("stops");
        strlcpy(config->berlinStopIds, stops.c_str(), sizeof(config->berlinStopIds));
    }
}

void ConfigWebServer::parseMqttSettings(Config* config)
{
    String selectedCity = String(config->city);

    // Only parse MQTT settings if city is MQTT
    if (selectedCity != "MQTT")
        return;

    if (server->hasArg("mqttBroker"))
        strlcpy(config->mqttBroker, server->arg("mqttBroker").c_str(), sizeof(config->mqttBroker));

    if (server->hasArg("mqttPort"))
    {
        config->mqttPort = server->arg("mqttPort").toInt();
        if (config->mqttPort < 1) config->mqttPort = 1;
        if (config->mqttPort > 65535) config->mqttPort = 65535;
    }

    if (server->hasArg("mqttUser"))
        strlcpy(config->mqttUsername, server->arg("mqttUser").c_str(), sizeof(config->mqttUsername));

    if (server->hasArg("mqttPass"))
        strlcpy(config->mqttPassword, server->arg("mqttPass").c_str(), sizeof(config->mqttPassword));

    if (server->hasArg("mqttReqTopic"))
        strlcpy(config->mqttRequestTopic, server->arg("mqttReqTopic").c_str(), sizeof(config->mqttRequestTopic));

    if (server->hasArg("mqttRespTopic"))
        strlcpy(config->mqttResponseTopic, server->arg("mqttRespTopic").c_str(), sizeof(config->mqttResponseTopic));

    if (server->hasArg("mqttEtaMode"))
        config->mqttUseEtaMode = (server->arg("mqttEtaMode") == "1");

    // Parse JSON field mappings
    if (server->hasArg("mqttFldLine"))
        strlcpy(config->mqttFieldLine, server->arg("mqttFldLine").c_str(), sizeof(config->mqttFieldLine));

    if (server->hasArg("mqttFldDest"))
        strlcpy(config->mqttFieldDestination, server->arg("mqttFldDest").c_str(), sizeof(config->mqttFieldDestination));

    if (server->hasArg("mqttFldEta"))
        strlcpy(config->mqttFieldEta, server->arg("mqttFldEta").c_str(), sizeof(config->mqttFieldEta));

    if (server->hasArg("mqttFldTime"))
        strlcpy(config->mqttFieldTimestamp, server->arg("mqttFldTime").c_str(), sizeof(config->mqttFieldTimestamp));

    if (server->hasArg("mqttFldPlat"))
        strlcpy(config->mqttFieldPlatform, server->arg("mqttFldPlat").c_str(), sizeof(config->mqttFieldPlatform));

    if (server->hasArg("mqttFldAC"))
        strlcpy(config->mqttFieldAC, server->arg("mqttFldAC").c_str(), sizeof(config->mqttFieldAC));
}

void ConfigWebServer::parseWeatherSettings(Config* config)
{
    // Weather enabled checkbox
    config->weatherEnabled = server->hasArg("weather_enabled");

    // Latitude
    if (server->hasArg("weather_lat"))
    {
        String latStr = server->arg("weather_lat");
        // Replace comma with dot for decimal separator (locale compatibility)
        latStr.replace(",", ".");
        config->weatherLatitude = latStr.toFloat();
        // Validate latitude range
        if (config->weatherLatitude < -90.0f)
            config->weatherLatitude = -90.0f;
        if (config->weatherLatitude > 90.0f)
            config->weatherLatitude = 90.0f;
    }

    // Longitude
    if (server->hasArg("weather_lon"))
    {
        String lonStr = server->arg("weather_lon");
        // Replace comma with dot for decimal separator (locale compatibility)
        lonStr.replace(",", ".");
        config->weatherLongitude = lonStr.toFloat();
        // Validate longitude range
        if (config->weatherLongitude < -180.0f)
            config->weatherLongitude = -180.0f;
        if (config->weatherLongitude > 180.0f)
            config->weatherLongitude = 180.0f;
    }

    // Refresh interval
    if (server->hasArg("weather_refresh"))
    {
        config->weatherRefreshInterval = server->arg("weather_refresh").toInt();
        // Clamp to 10-60 minutes
        if (config->weatherRefreshInterval < 10)
            config->weatherRefreshInterval = 10;
        if (config->weatherRefreshInterval > 60)
            config->weatherRefreshInterval = 60;
    }
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
    html += "<h1>Rebooting...</h1>";
    html += "<p>The device is rebooting. Please wait 10-15 seconds for it to come back online.</p>";
    html += "<div id='reconnect-msg' style='display:none; margin-top:20px;'>";
    html += "<p><button onclick='window.location=\"/\"' style='padding:12px 24px; font-size:16px; cursor:pointer; background:#2ed573; color:#000; border:none; border-radius:8px;'>Reconnect to Device</button></p>";
    html += "</div>";
    html += "<script>setTimeout(function(){ document.getElementById('reconnect-msg').style.display='block'; }, 10000);</script>";
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
    html += "<h1>Clearing All Settings...</h1>";
    html += "<div class='card' style='background: #ff6b6b; color: #fff;'>";
    html += "<p>All configuration has been erased from flash memory.</p>";
    html += "<p>The device will reboot into AP (setup) mode in 10 seconds.</p>";
    html += "<p>You will need to reconfigure WiFi and API settings.</p>";
    html += "</div>";
    html += "<div id='reconnect-msg' style='display:none; margin-top:20px;'>";
    html += "<p><strong>Device should now be in AP mode.</strong></p>";
    html += "<p>Look for a WiFi network starting with: <strong>SpojBoard-XXXX</strong></p>";
    html += "</div>";
    html += "<script>setTimeout(function(){ document.getElementById('reconnect-msg').style.display='block'; }, 15000);</script>";
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

        // Copy to departure structure
        strlcpy(demoDepartures[demoCount].line, lineValue.c_str(), sizeof(demoDepartures[demoCount].line));
        strlcpy(demoDepartures[demoCount].destination, destValue.c_str(), sizeof(demoDepartures[demoCount].destination));
        demoDepartures[demoCount].eta = etaValue.toInt();
        strlcpy(demoDepartures[demoCount].platform, platformValue.c_str(), sizeof(demoDepartures[demoCount].platform));
        demoDepartures[demoCount].hasAC = hasAC;
        demoDepartures[demoCount].isDelayed = false;
        demoDepartures[demoCount].delayMinutes = 0;
        demoDepartures[demoCount].departureTime = 0;  // Not used in demo mode

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

void ConfigWebServer::handleDisplayStateAPI()
{
    if (!displayManager)
    {
        server->send(500, "application/json", "{\"error\":\"Display not initialized\"}");
        return;
    }

    // Get current display data
    const Departure* departures = displayManager->getCurrentDepartures();
    int numToDisplay = displayManager->getCurrentNumToDisplay();
    int departureCount = displayManager->getCurrentDepartureCount();
    const WeatherData* weather = displayManager->getWeatherData();

    // Limit to actual display capacity (max 3 rows, or numToDisplay if less)
    int rowsToShow = (departureCount < numToDisplay) ? departureCount : numToDisplay;
    if (rowsToShow > 3)
        rowsToShow = 3;

    // Check if API key is configured based on city
    bool apiKeyConfigured = false;
    if (currentConfig)
    {
        if (strcmp(currentConfig->city, "Prague") == 0)
        {
            apiKeyConfigured = (currentConfig->pragueApiKey[0] != '\0' && currentConfig->pragueStopIds[0] != '\0');
        }
        else if (strcmp(currentConfig->city, "Berlin") == 0)
        {
            apiKeyConfigured = (currentConfig->berlinStopIds[0] != '\0');
        }
        else if (strcmp(currentConfig->city, "MQTT") == 0)
        {
            apiKeyConfigured = (currentConfig->mqttBroker[0] != '\0');
        }
    }

    // Build JSON response using ApiHandlers
    String json = buildDisplayStateJson(
        departures, rowsToShow, weather,
        apModeActive, wifiConnected, apiKeyConfigured,
        apiError, apiErrorMsg, demoModeActive,
        restModeActive, restModeManual, departureCount,
        stopName, apSSID, apPassword
    );

    server->send(200, "application/json", json);
}

void ConfigWebServer::handlePreviewPage()
{
    String html = buildPreviewPage();
    server->send(200, "text/html", html);
}
