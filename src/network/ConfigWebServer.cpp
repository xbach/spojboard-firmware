#include "ConfigWebServer.h"
#include "../utils/Logger.h"
#include "../display/DisplayManager.h"
#include "../api/WeatherAPI.h"
#include "web/WebTemplates.h"
#include "web/DashboardPage.h"
#include "web/DemoPage.h"
#include "web/UpdatePage.h"
#include <WiFi.h>
#include <Update.h>
#include <string.h>

// Helper function to count stops in comma-separated list
static int countStops(const char* stopIds) {
    if (!stopIds || stopIds[0] == '\0') {
        return 0;
    }

    int count = 1;  // At least one stop if string is not empty
    for (const char* p = stopIds; *p; p++) {
        if (*p == ',') {
            count++;
        }
    }
    return count;
}

// Helper function to escape JSON strings
static String escapeJsonString(const char *str)
{
    String result = "";
    if (!str)
        return result;

    for (size_t i = 0; str[i] != '\0'; i++)
    {
        char c = str[i];
        switch (c)
        {
        case '"':
            result += "\\\"";
            break;
        case '\\':
            result += "\\\\";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        case '\b':
            result += "\\b";
            break;
        case '\f':
            result += "\\f";
            break;
        default:
            // Skip other control characters
            if (c >= 0 && c < 32)
            {
                // Skip control characters
            }
            else
            {
                result += c;
            }
            break;
        }
    }
    return result;
}

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

    if (server->hasArg("ssid"))
    {
        String newSsid = server->arg("ssid");
        if (newSsid != newConfig.wifiSsid)
        {
            wifiChanged = true;
        }
        strlcpy(newConfig.wifiSsid, newSsid.c_str(), sizeof(newConfig.wifiSsid));
    }
    if (server->hasArg("password") && server->arg("password").length() > 0)
    {
        strlcpy(newConfig.wifiPassword, server->arg("password").c_str(), sizeof(newConfig.wifiPassword));
        wifiChanged = true;
    }
    // Parse city field
    if (server->hasArg("city"))
    {
        String newCity = server->arg("city");
        // Validate city value
        if (newCity == "Berlin" || newCity == "Prague" || newCity == "MQTT")
        {
            if (newCity != newConfig.city)
            {
                cityChanged = true;
            }
            strlcpy(newConfig.city, newCity.c_str(), sizeof(newConfig.city));
        }
        else
        {
            // Invalid city value, default to Prague
            strlcpy(newConfig.city, "Prague", sizeof(newConfig.city));
        }
    }
    // Save API key and stops to appropriate city-specific fields
    String selectedCity = String(newConfig.city);

    if (server->hasArg("apikey") && server->arg("apikey").length() > 0)
    {
        String apiKeyValue = server->arg("apikey");
        // Only save if it's not the placeholder dots (visual feedback, not actual key)
        if (apiKeyValue != "****" && selectedCity == "Prague")
        {
            strlcpy(newConfig.pragueApiKey, apiKeyValue.c_str(), sizeof(newConfig.pragueApiKey));
        }
    }

    if (server->hasArg("stops"))
    {
        String stops = server->arg("stops");

        // Validate maximum number of stops (with 1s delay per stop, 12 stops = 12s+ query time)
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

        // Save to city-specific field
        if (selectedCity == "Prague")
        {
            strlcpy(newConfig.pragueStopIds, stops.c_str(), sizeof(newConfig.pragueStopIds));
        }
        else if (selectedCity == "Berlin")
        {
            strlcpy(newConfig.berlinStopIds, stops.c_str(), sizeof(newConfig.berlinStopIds));
        }
        // MQTT doesn't use stops field
    }

    // Parse MQTT-specific fields (when city is MQTT)
    if (selectedCity == "MQTT")
    {
        if (server->hasArg("mqttBroker"))
            strlcpy(newConfig.mqttBroker, server->arg("mqttBroker").c_str(), sizeof(newConfig.mqttBroker));

        if (server->hasArg("mqttPort"))
        {
            newConfig.mqttPort = server->arg("mqttPort").toInt();
            if (newConfig.mqttPort < 1) newConfig.mqttPort = 1;
            if (newConfig.mqttPort > 65535) newConfig.mqttPort = 65535;
        }

        if (server->hasArg("mqttUser"))
            strlcpy(newConfig.mqttUsername, server->arg("mqttUser").c_str(), sizeof(newConfig.mqttUsername));

        if (server->hasArg("mqttPass"))
            strlcpy(newConfig.mqttPassword, server->arg("mqttPass").c_str(), sizeof(newConfig.mqttPassword));

        if (server->hasArg("mqttReqTopic"))
            strlcpy(newConfig.mqttRequestTopic, server->arg("mqttReqTopic").c_str(), sizeof(newConfig.mqttRequestTopic));

        if (server->hasArg("mqttRespTopic"))
            strlcpy(newConfig.mqttResponseTopic, server->arg("mqttRespTopic").c_str(), sizeof(newConfig.mqttResponseTopic));

        if (server->hasArg("mqttEtaMode"))
            newConfig.mqttUseEtaMode = (server->arg("mqttEtaMode") == "1");

        // Parse JSON field mappings
        if (server->hasArg("mqttFldLine"))
            strlcpy(newConfig.mqttFieldLine, server->arg("mqttFldLine").c_str(), sizeof(newConfig.mqttFieldLine));

        if (server->hasArg("mqttFldDest"))
            strlcpy(newConfig.mqttFieldDestination, server->arg("mqttFldDest").c_str(), sizeof(newConfig.mqttFieldDestination));

        if (server->hasArg("mqttFldEta"))
            strlcpy(newConfig.mqttFieldEta, server->arg("mqttFldEta").c_str(), sizeof(newConfig.mqttFieldEta));

        if (server->hasArg("mqttFldTime"))
            strlcpy(newConfig.mqttFieldTimestamp, server->arg("mqttFldTime").c_str(), sizeof(newConfig.mqttFieldTimestamp));

        if (server->hasArg("mqttFldPlat"))
            strlcpy(newConfig.mqttFieldPlatform, server->arg("mqttFldPlat").c_str(), sizeof(newConfig.mqttFieldPlatform));

        if (server->hasArg("mqttFldAC"))
            strlcpy(newConfig.mqttFieldAC, server->arg("mqttFldAC").c_str(), sizeof(newConfig.mqttFieldAC));
    }

    if (server->hasArg("refresh"))
    {
        newConfig.refreshInterval = server->arg("refresh").toInt();
        if (newConfig.refreshInterval < 10)
            newConfig.refreshInterval = 10;
        if (newConfig.refreshInterval > 300)
            newConfig.refreshInterval = 300;
    }
    if (server->hasArg("numdeps"))
    {
        newConfig.numDepartures = server->arg("numdeps").toInt();
        if (newConfig.numDepartures < 1)
            newConfig.numDepartures = 1;
        if (newConfig.numDepartures > 3)
            newConfig.numDepartures = 3;  // Max 3 rows on display
    }
    if (server->hasArg("mindeptime"))
    {
        newConfig.minDepartureTime = server->arg("mindeptime").toInt();
        if (newConfig.minDepartureTime < 0)
            newConfig.minDepartureTime = 0;
        if (newConfig.minDepartureTime > 30)
            newConfig.minDepartureTime = 30;
    }
    if (server->hasArg("brightness"))
    {
        newConfig.brightness = server->arg("brightness").toInt();
        if (newConfig.brightness < 0)
            newConfig.brightness = 0;
        if (newConfig.brightness > 255)
            newConfig.brightness = 255;
    }
    if (server->hasArg("language"))
    {
        String lang = server->arg("language");
        if (lang == "cs" || lang == "de" || lang == "en")
        {
            strlcpy(newConfig.language, lang.c_str(), sizeof(newConfig.language));
        }
        else
        {
            strlcpy(newConfig.language, "en", sizeof(newConfig.language));
        }
    }

    // Debug mode checkbox (unchecked = not present in POST data)
    newConfig.debugMode = server->hasArg("debugmode");

    // Show platform checkbox (unchecked = not present in POST data)
    newConfig.showPlatform = server->hasArg("showplatform");

    // Scrolling checkbox (unchecked = not present in POST data)
    newConfig.scrollEnabled = server->hasArg("scrollenabled");

    // Weather configuration
    newConfig.weatherEnabled = server->hasArg("weather_enabled");

    if (server->hasArg("weather_lat"))
    {
        String latStr = server->arg("weather_lat");
        // Replace comma with dot for decimal separator (locale compatibility)
        latStr.replace(",", ".");
        newConfig.weatherLatitude = latStr.toFloat();
        // Validate latitude range
        if (newConfig.weatherLatitude < -90.0f)
            newConfig.weatherLatitude = -90.0f;
        if (newConfig.weatherLatitude > 90.0f)
            newConfig.weatherLatitude = 90.0f;
    }

    if (server->hasArg("weather_lon"))
    {
        String lonStr = server->arg("weather_lon");
        // Replace comma with dot for decimal separator (locale compatibility)
        lonStr.replace(",", ".");
        newConfig.weatherLongitude = lonStr.toFloat();
        // Validate longitude range
        if (newConfig.weatherLongitude < -180.0f)
            newConfig.weatherLongitude = -180.0f;
        if (newConfig.weatherLongitude > 180.0f)
            newConfig.weatherLongitude = 180.0f;
    }

    if (server->hasArg("weather_refresh"))
    {
        newConfig.weatherRefreshInterval = server->arg("weather_refresh").toInt();
        // Clamp to 10-60 minutes
        if (newConfig.weatherRefreshInterval < 10)
            newConfig.weatherRefreshInterval = 10;
        if (newConfig.weatherRefreshInterval > 60)
            newConfig.weatherRefreshInterval = 60;
    }

    // Line color map (always update when not in AP mode to handle empty case)
    if (!apModeActive)
    {
        // Get the value, defaulting to empty string if not present
        String colorMapValue = server->hasArg("linecolormap")
                               ? server->arg("linecolormap")
                               : "";

        strlcpy(newConfig.lineColorMap, colorMapValue.c_str(), sizeof(newConfig.lineColorMap));

        // Log configuration
        logTimestamp();
        Serial.print("Line color map updated: ");
        Serial.println(strlen(newConfig.lineColorMap) > 0 ? newConfig.lineColorMap : "(empty - using defaults)");
    }

    // Parse rest mode periods
    if (server->hasArg("restperiods"))
    {
        String restPeriods = server->arg("restperiods");

        if (restPeriods.length() < sizeof(newConfig.restModePeriods))
        {
            strlcpy(newConfig.restModePeriods, restPeriods.c_str(), sizeof(newConfig.restModePeriods));
        }
        else
        {
            logTimestamp();
            debugPrintln("RestMode: Config string too long, truncating");
            strlcpy(newConfig.restModePeriods, restPeriods.c_str(), sizeof(newConfig.restModePeriods));
        }
    }

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

    // Build JSON response with properly escaped strings
    String json = "{";

    if (info.hasError)
    {
        json += "\"available\":false,";
        json += "\"error\":\"" + escapeJsonString(info.errorMsg) + "\"";
    }
    else if (info.available)
    {
        json += "\"available\":true,";
        json += "\"releaseNumber\":" + String(info.releaseNumber) + ",";
        json += "\"releaseName\":\"" + escapeJsonString(info.releaseName) + "\",";
        json += "\"releaseNotes\":\"" + escapeJsonString(info.releaseNotes) + "\",";
        json += "\"fileName\":\"" + escapeJsonString(info.assetName) + "\",";
        json += "\"fileSize\":" + String(info.assetSize) + ",";
        json += "\"assetUrl\":\"" + escapeJsonString(info.assetUrl) + "\"";
    }
    else
    {
        json += "\"available\":false";
    }

    json += "}";

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
    int departureCount = displayManager->getCurrentDepartureCount();
    const WeatherData* weather = displayManager->getWeatherData();

    // Build JSON response
    String json = "{";
    json += "\"success\":true,";
    json += "\"timestamp\":" + String(millis()) + ",";

    // Current time
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    char timeStr[32];
    strftime(timeStr, sizeof(timeStr), "%a %d.%b %H:%M", timeinfo);
    json += "\"time\":\"" + String(timeStr) + "\",";

    // Weather (if enabled and available)
    if (weather && weather->temperature != 0)
    {
        json += "\"weather\":{";
        json += "\"temp\":" + String(weather->temperature) + ",";
        json += "\"code\":" + String(weather->weatherCode);
        json += "},";
    }

    // Departures array
    json += "\"departures\":[";
    for (int i = 0; i < departureCount; i++)
    {
        if (i > 0)
            json += ",";
        json += "{";
        json += "\"line\":\"" + String(departures[i].line) + "\",";
        json += "\"destination\":\"" + String(departures[i].destination) + "\",";
        json += "\"eta\":" + String(departures[i].eta) + ",";
        json += "\"hasAC\":" + String(departures[i].hasAC ? "true" : "false") + ",";
        json += "\"isDelayed\":" + String(departures[i].isDelayed ? "true" : "false");
        json += "}";
    }
    json += "]";

    json += "}";

    server->send(200, "application/json", json);
}

void ConfigWebServer::handlePreviewPage()
{
    String html = FPSTR(HTML_HEADER);
    html += "<h1>Display Preview</h1>";
    html += "<p style='text-align:center; color:#888; margin-top:-10px; margin-bottom:20px;'>Live view of LED matrix display</p>";

    // CSS Styles for LED Matrix Replica
    html += "<style>";
    html += ".led-display { ";
    html += "  background: #000; ";
    html += "  border: 3px solid #333; ";
    html += "  padding: 10px; ";
    html += "  margin: 20px auto; ";
    html += "  max-width: 800px; ";
    html += "  font-family: 'Courier New', monospace; ";
    html += "  box-shadow: 0 0 20px rgba(0,0,0,0.5); ";
    html += "}";
    html += ".led-row { ";
    html += "  display: flex; ";
    html += "  align-items: center; ";
    html += "  height: 40px; ";
    html += "  margin: 2px 0; ";
    html += "  padding: 4px 8px; ";
    html += "}";
    html += ".led-line-box { ";
    html += "  min-width: 45px; ";
    html += "  height: 28px; ";
    html += "  background: #000; ";
    html += "  border: 1px solid #333; ";
    html += "  display: flex; ";
    html += "  align-items: center; ";
    html += "  justify-content: center; ";
    html += "  font-size: 14px; ";
    html += "  font-weight: bold; ";
    html += "  margin-right: 8px; ";
    html += "  text-shadow: 0 0 3px currentColor; ";
    html += "}";
    html += ".led-destination { ";
    html += "  flex: 1; ";
    html += "  color: #fff; ";
    html += "  font-size: 14px; ";
    html += "  white-space: nowrap; ";
    html += "  overflow: hidden; ";
    html += "  text-overflow: ellipsis; ";
    html += "  text-shadow: 0 0 2px #fff; ";
    html += "}";
    html += ".led-eta { ";
    html += "  color: #fff; ";
    html += "  font-size: 14px; ";
    html += "  font-weight: bold; ";
    html += "  margin-left: auto; ";
    html += "  padding-left: 10px; ";
    html += "  text-shadow: 0 0 2px #fff; ";
    html += "}";
    html += ".led-status { ";
    html += "  display: flex; ";
    html += "  justify-content: space-between; ";
    html += "  color: #fff; ";
    html += "  font-size: 12px; ";
    html += "  padding: 8px; ";
    html += "  border-top: 1px solid #333; ";
    html += "  text-shadow: 0 0 2px #fff; ";
    html += "}";

    // Color classes for line boxes
    html += ".color-red { color: #ff0000; }";
    html += ".color-green { color: #00ff00; }";
    html += ".color-blue { color: #0000ff; }";
    html += ".color-yellow { color: #ffff00; }";
    html += ".color-orange { color: #ff8800; }";
    html += ".color-purple { color: #aa00ff; }";
    html += ".color-cyan { color: #00ffff; }";
    html += ".color-white { color: #ffffff; }";

    // ETA color classes
    html += ".eta-normal { color: #ffffff; }";
    html += ".eta-soon { color: #ffff00; }";
    html += ".eta-urgent { color: #ff0000; }";
    html += ".eta-delayed { color: #ff8800; }";

    // Weather colors
    html += ".weather-cold { color: #0088ff; }";
    html += ".weather-mild { color: #ffffff; }";
    html += ".weather-warm { color: #ffff00; }";
    html += ".weather-hot { color: #ff0000; }";

    html += ".controls { text-align: center; margin: 20px; }";
    html += ".controls button { ";
    html += "  padding: 10px 20px; ";
    html += "  margin: 5px; ";
    html += "  font-size: 14px; ";
    html += "  cursor: pointer; ";
    html += "  background: #2ed573; ";
    html += "  color: #000; ";
    html += "  border: none; ";
    html += "  border-radius: 8px; ";
    html += "}";
    html += ".controls button:hover { background: #26de81; }";
    html += "#status { text-align: center; color: #888; margin: 10px; }";
    html += "</style>";

    // LED Display Container
    html += "<div class='led-display' id='ledDisplay'>";
    html += "<div id='departureRows'></div>";
    html += "<div class='led-status' id='statusBar'></div>";
    html += "</div>";

    // Controls
    html += "<div class='controls'>";
    html += "<button onclick='toggleAutoRefresh()' id='toggleBtn'>Pause</button>";
    html += "<button onclick='location.href=\"/\"'>Back to Dashboard</button>";
    html += "</div>";
    html += "<p id='status'>Loading...</p>";

    // JavaScript
    html += "<script>";

    // Color mapping function (matches DisplayColors.cpp logic)
    html += "function getLineColor(line) {";
    html += "  if (line === 'A') return 'green';";
    html += "  if (line === 'B') return 'yellow';";
    html += "  if (line === 'C') return 'red';";
    html += "  if (/^S\\d+$/.test(line)) return 'blue';";  // S-trains
    html += "  if (/^9[1-9]$/.test(line)) return 'cyan';";  // Night trams
    html += "  if (/^[1-2]\\d$/.test(line)) return 'white';";  // Trams
    html += "  if (/^(5[0-9]|[1-2]\\d\\d)$/.test(line)) return 'purple';";  // Buses
    html += "  if (/^9\\d\\d$/.test(line)) return 'cyan';";  // Night buses
    html += "  return 'yellow';";  // Default
    html += "}";

    // ETA color function
    html += "function getEtaColor(eta, isDelayed) {";
    html += "  if (isDelayed) return 'eta-delayed';";
    html += "  if (eta < 2) return 'eta-urgent';";
    html += "  if (eta <= 5) return 'eta-soon';";
    html += "  return 'eta-normal';";
    html += "}";

    // Weather color function
    html += "function getWeatherTempColor(temp) {";
    html += "  if (temp < 8) return 'weather-cold';";
    html += "  if (temp <= 16) return 'weather-mild';";
    html += "  if (temp <= 25) return 'weather-warm';";
    html += "  return 'weather-hot';";
    html += "}";

    // Weather icon mapping (using emoji approximations)
    html += "function getWeatherIcon(code) {";
    html += "  if (code === 0) return '\\u2600';";  // Clear sky - sun
    html += "  if (code <= 3) return '\\u2601';";  // Cloudy
    html += "  if (code <= 48) return '\\u2248';";  // Fog - approximately equal
    html += "  if (code <= 57) return '\\u2022';";  // Drizzle - bullet
    html += "  if (code <= 67) return '\\u2248';";  // Rain - approximately equal
    html += "  if (code <= 86) return '\\u2744';";  // Snow
    html += "  if (code >= 95) return '\\u26C8';";  // Thunderstorm
    html += "  return '\\u2601';";  // Default cloudy
    html += "}";

    // Update display function
    html += "async function updateDisplay() {";
    html += "  try {";
    html += "    const response = await fetch('/api/display-state');";
    html += "    const data = await response.json();";
    html += "    if (!data.success) throw new Error('API call failed');";

    // Render departures
    html += "    const rowsHtml = data.departures.map(dep => {";
    html += "      const color = getLineColor(dep.line);";
    html += "      const etaColor = getEtaColor(dep.eta, dep.isDelayed);";
    html += "      const etaText = dep.eta < 1 ? '<1' : dep.eta;";
    html += "      const acIndicator = dep.hasAC ? '*' : '';";
    html += "      return `<div class='led-row'>` +";
    html += "        `<div class='led-line-box color-${color}'>${dep.line}</div>` +";
    html += "        `<div class='led-destination'>${dep.destination}${acIndicator}</div>` +";
    html += "        `<div class='led-eta ${etaColor}'>${etaText}\\'</div>` +";
    html += "      `</div>`;";
    html += "    }).join('');";
    html += "    document.getElementById('departureRows').innerHTML = rowsHtml;";

    // Render status bar
    html += "    let statusHtml = `<span>${data.time}</span>`;";
    html += "    if (data.weather) {";
    html += "      const icon = getWeatherIcon(data.weather.code);";
    html += "      const tempColor = getWeatherTempColor(data.weather.temp);";
    html += "      statusHtml += `<span>${icon} <span class='${tempColor}'>${data.weather.temp}\u00B0</span></span>`;";
    html += "    }";
    html += "    document.getElementById('statusBar').innerHTML = statusHtml;";

    html += "    document.getElementById('status').textContent = 'Updated: ' + new Date().toLocaleTimeString();";
    html += "    document.getElementById('status').style.color = '#2ed573';";
    html += "  } catch (err) {";
    html += "    document.getElementById('status').textContent = 'Error: ' + err.message;";
    html += "    document.getElementById('status').style.color = '#ff6b6b';";
    html += "  }";
    html += "}";

    // Auto-refresh logic
    html += "let autoRefresh = true;";
    html += "let refreshInterval = null;";
    html += "function toggleAutoRefresh() {";
    html += "  autoRefresh = !autoRefresh;";
    html += "  const btn = document.getElementById('toggleBtn');";
    html += "  btn.textContent = autoRefresh ? 'Pause' : 'Resume';";
    html += "  if (autoRefresh) startAutoRefresh(); else stopAutoRefresh();";
    html += "}";
    html += "function startAutoRefresh() {";
    html += "  if (refreshInterval) clearInterval(refreshInterval);";
    html += "  refreshInterval = setInterval(updateDisplay, 3000);";
    html += "}";
    html += "function stopAutoRefresh() {";
    html += "  if (refreshInterval) clearInterval(refreshInterval);";
    html += "}";

    // Initialize
    html += "updateDisplay();";
    html += "startAutoRefresh();";
    html += "</script>";

    html += FPSTR(HTML_FOOTER);

    server->send(200, "text/html", html);
}
