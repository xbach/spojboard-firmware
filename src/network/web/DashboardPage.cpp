#include "DashboardPage.h"
#include "WebTemplates.h"
#include "ClientScripts.h"
#include <WiFi.h>

String buildDashboardPage(
    const Config* config,
    bool apModeActive,
    bool wifiConnected,
    const char* apSSID,
    int apClientCount,
    bool apiError,
    const char* apiErrorMsg,
    int departureCount,
    const char* stopName)
{
    String html = FPSTR(HTML_HEADER);
    html += "<h1>SpojBoard</h1>";
    html += "<p style='text-align:center; color:#888; margin-top:-10px; margin-bottom:20px;'>Smart Panel for Onward Journeys</p>";

    // AP Mode banner
    if (apModeActive)
    {
        html += "<div class='card' style='background: #ff6b6b; color: #fff;'>";
        html += "<h2 style='color: #fff; margin-top: 0;'>Setup Mode</h2>";
        html += "<p>Device is in Access Point mode. Configure WiFi credentials below to connect to your network.</p>";
        html += "<p><strong>AP Name:</strong> " + String(apSSID) + "</p>";
        html += "</div>";
    }

    // Status card
    html += "<div class='card'>";
    html += "<h2>Status</h2>";

    if (apModeActive)
    {
        html += "<div class='status warn'>AP Mode Active - Not connected to WiFi</div>";
        html += "<p><strong>Connected clients:</strong> " + String(apClientCount) + "</p>";
    }
    else if (wifiConnected)
    {
        html += "<div class='status ok'>WiFi Connected: " + WiFi.localIP().toString() + "</div>";
    }
    else
    {
        html += "<div class='status error'>WiFi Disconnected</div>";
    }

    if (!apModeActive)
    {
        // Display current city's configuration status
        bool isPrague = (strcmp(config->city, "Berlin") != 0 && strcmp(config->city, "MQTT") != 0);
        bool isMqtt = (strcmp(config->city, "MQTT") == 0);
        bool hasApiKey = isPrague ? (strlen(config->pragueApiKey) > 0) : true;
        bool hasStops = isPrague ? (strlen(config->pragueStopIds) > 0) :
                        isMqtt ? true : (strlen(config->berlinStopIds) > 0);

        if (hasApiKey && hasStops)
        {
            if (apiError)
            {
                html += "<div class='status error'>API Error: " + String(apiErrorMsg) + "</div>";
            }
            else
            {
                html += "<div class='status ok'>API OK - " + String(departureCount) + " departures</div>";
            }

            if (isPrague)
            {
                html += "<p><strong>Prague API Key:</strong> Configured (hidden)</p>";
            }
            else if (isMqtt)
            {
                html += "<p><strong>MQTT:</strong> " + String(config->mqttBroker) + ":" + String(config->mqttPort) + "</p>";
            }
            else
            {
                html += "<p><strong>Berlin API:</strong> No authentication required</p>";
            }
        }
        else if (!hasApiKey && isPrague)
        {
            html += "<div class='status warn'>Prague API Key not configured</div>";
        }
        else if (!hasStops)
        {
            html += "<div class='status warn'>Stop IDs not configured</div>";
        }

        if (stopName && stopName[0])
        {
            html += "<p><strong>Stop:</strong> " + String(stopName) + "</p>";
        }
    }

    html += "<p><strong>Free Memory:</strong> " + String(ESP.getFreeHeap()) + " bytes</p>";

    // Format firmware version with build ID
    char buildIdStr[10];
    snprintf(buildIdStr, sizeof(buildIdStr), "%08x", BUILD_ID);
    html += "<p><strong>Firmware:</strong> Release " + String(FIRMWARE_RELEASE) + " (" + String(buildIdStr) + ")</p>";
    html += "</div>";

    // Configuration form
    html += "<div class='card'>";
    html += "<h2>Configuration</h2>";
    html += "<form method='POST' action='/save'>";

    html += "<label>WiFi SSID</label>";
    html += "<input type='text' name='ssid' value='" + String(config->wifiSsid) + "' required placeholder='Your WiFi network name'>";

    html += "<label>WiFi Password</label>";
    html += "<input type='password' name='password' placeholder='Enter WiFi password'>";
    if (!apModeActive)
    {
        html += "<p class='info'>Leave empty to keep current password</p>";
    }
    else
    {
        html += "<p class='info'>Enter your WiFi password</p>";
    }

    // City selector
    html += "<label>Transit City</label>";
    html += "<select name='city' id='citySelect' onchange='switchCity()' required>";
    bool isMqtt = (strcmp(config->city, "MQTT") == 0);
    bool isBerlin = (strcmp(config->city, "Berlin") == 0);
    bool isPrague = !isMqtt && !isBerlin;
    html += "<option value='Prague'" + String(isPrague ? " selected" : "") + ">Prague (PID/Golemio)</option>";
    html += "<option value='Berlin'" + String(isBerlin ? " selected" : "") + ">Berlin (BVG)</option>";
    html += "<option value='MQTT'" + String(isMqtt ? " selected" : "") + ">MQTT (Custom)</option>";
    html += "</select>";
    html += "<p class='info'>Select your transit network. Device will restart after changing city.</p>";

    // Hidden inputs to store per-city data
    html += "<input type='hidden' id='pragueApiKeyData' value='" + String(config->pragueApiKey) + "'>";
    html += "<input type='hidden' id='pragueStopsData' value='" + String(config->pragueStopIds) + "'>";
    html += "<input type='hidden' id='berlinStopsData' value='" + String(config->berlinStopIds) + "'>";

    // API Key field (Prague only)
    html += "<div id='apiKeySection'>";
    html += "<label><span id='apiKeyLabel'>Prague API Key (Golemio)</span></label>";
    bool hasPragueKey = strlen(config->pragueApiKey) > 0;
    html += "<input type='password' name='apikey' id='apiKeyInput' placeholder='" + String(hasPragueKey ? "****" : "Enter API key") + "' value=''>";
    if (apModeActive)
    {
        html += "<p class='info' id='apiKeyHelp'>Get your API key at <a href='https://api.golemio.cz/api-keys/' target='_blank'>api.golemio.cz</a>. Try the demo first!</p>";
    }
    else if (hasPragueKey)
    {
        html += "<p class='info' id='apiKeyHelp'>API key configured. Leave empty to keep current key, or enter a new key to replace it. Get keys at <a href='https://api.golemio.cz/api-keys/' target='_blank'>api.golemio.cz</a></p>";
    }
    else
    {
        html += "<p class='info' id='apiKeyHelp'>Required: Get your API key at <a href='https://api.golemio.cz/api-keys/' target='_blank'>api.golemio.cz</a></p>";
    }
    html += "</div>";

    // MQTT Configuration Section
    html += "<div id='mqttSection' style='display:none;'>";
    html += "<h3>MQTT Broker Settings</h3>";
    html += "<label>MQTT Broker Address</label>";
    html += "<input type='text' name='mqttBroker' id='mqttBrokerInput' placeholder='192.168.1.100 or mqtt.example.com' value='" + String(config->mqttBroker) + "'>";
    html += "<p class='info'>IP address or hostname of MQTT broker</p>";

    html += "<div class='grid'>";
    html += "<div><label>MQTT Broker Port</label>";
    html += "<input type='number' name='mqttPort' id='mqttPortInput' min='1' max='65535' value='" + String(config->mqttPort) + "'></div>";

    html += "<div><label>MQTT Username (optional)</label>";
    html += "<input type='text' name='mqttUser' id='mqttUserInput' placeholder='Leave empty for no auth' value='" + String(config->mqttUsername) + "'></div>";
    html += "</div>";

    html += "<label>MQTT Password (optional)</label>";
    html += "<input type='password' name='mqttPass' id='mqttPassInput' placeholder='Leave empty for no auth' value='" + String(config->mqttPassword) + "'>";
    html += "<p class='info'>Leave username empty for no authentication</p>";

    html += "<div class='grid'>";
    html += "<div><label>Request Topic</label>";
    html += "<input type='text' name='mqttReqTopic' id='mqttReqTopicInput' placeholder='transit/request' value='" + String(config->mqttRequestTopic) + "'></div>";

    html += "<div><label>Response Topic</label>";
    html += "<input type='text' name='mqttRespTopic' id='mqttRespTopicInput' placeholder='transit/response' value='" + String(config->mqttResponseTopic) + "'></div>";
    html += "</div>";

    html += "<label>ETA Mode</label>";
    html += "<select name='mqttEtaMode' id='mqttEtaModeInput' onchange='updateEtaModeHelp()'>";
    html += "<option value='0'" + String(!config->mqttUseEtaMode ? " selected" : "") + ">Timestamp Mode (Unix timestamp, recalculated every 10s)</option>";
    html += "<option value='1'" + String(config->mqttUseEtaMode ? " selected" : "") + ">ETA Mode (Pre-calculated minutes, no recalc)</option>";
    html += "</select>";
    html += "<p class='info'>Choose how departure times are provided by your MQTT source</p>";
    html += "<p id='etaModeHelp' class='info' style='background-color:#2a3f5f; padding:10px; border-radius:4px; margin-top:8px;'></p>";

    html += "<h3>JSON Field Mappings</h3>";
    html += "<p class='info'>Configure field names in your MQTT JSON response. Defaults match example format.</p>";
    html += "<div class='grid'>";
    html += "<div><label>Line Number Field</label>";
    html += "<input type='text' name='mqttFldLine' value='" + String(config->mqttFieldLine) + "' placeholder='line'></div>";

    html += "<div><label>Destination Field</label>";
    html += "<input type='text' name='mqttFldDest' value='" + String(config->mqttFieldDestination) + "' placeholder='dest'></div>";

    html += "<div><label>ETA Field (minutes)</label>";
    html += "<input type='text' name='mqttFldEta' value='" + String(config->mqttFieldEta) + "' placeholder='eta'></div>";

    html += "<div><label>Timestamp Field (unix)</label>";
    html += "<input type='text' name='mqttFldTime' value='" + String(config->mqttFieldTimestamp) + "' placeholder='dep'></div>";

    html += "<div><label>Platform Field (optional)</label>";
    html += "<input type='text' name='mqttFldPlat' value='" + String(config->mqttFieldPlatform) + "' placeholder='plt'></div>";

    html += "<div><label>AC Flag Field (optional)</label>";
    html += "<input type='text' name='mqttFldAC' value='" + String(config->mqttFieldAC) + "' placeholder='ac'></div>";
    html += "</div>";
    html += "<p class='info'><strong>Note:</strong> For MQTT, configure minimum departure time filtering on your server to keep responses minimal.</p>";
    html += "</div>"; // End mqttSection

    html += "<label>Stop ID(s)</label>";
    html += "<input type='text' name='stops' id='stopsInput' value='" + String(isPrague ? config->pragueStopIds : config->berlinStopIds) + "' required placeholder='e.g., U693Z2P (Prague) or 900013102 (Berlin)'>";
    html += "<p class='info' id='stopHelp'>";
    if (isPrague)
    {
        html += "Comma-separated PID stop IDs (e.g., U693Z2P). Find IDs at <a href='https://data.pid.cz/stops/json/stops.json' target='_blank'>PID data</a>";
    }
    else
    {
        html += "Comma-separated numeric BVG stop IDs (e.g., 900013102). Find IDs at <a href='https://v6.bvg.transport.rest/' target='_blank'>BVG API</a>";
    }
    html += "</p>";

    html += "<div class='grid'>";
    html += "<div><label>Refresh Interval (sec)</label>";
    html += "<input type='number' name='refresh' value='" + String(config->refreshInterval) + "' min='10' max='300'></div>";

    html += "<div><label>Number of Departures to Display (1-3 rows)</label>";
    html += "<input type='number' name='numdeps' value='" + String(config->numDepartures) + "' min='1' max='3'></div>";

    html += "<div><label>Min Departure Time (min)</label>";
    html += "<input type='number' name='mindeptime' id='minDepTimeInput' value='" + String(config->minDepartureTime) + "' min='0' max='30'>";
    html += "<p id='minDepTimeHelp' class='info' style='display:none; margin-top:5px; font-size:0.9em;'><strong>MQTT:</strong> Set this value on both server (initial filter) and device (recalculation filter). Server filters at send time, device filters during 10s recalcs.</p>";
    html += "</div>";

    html += "<div><label>Display Brightness (0-255)</label>";
    html += "<input type='number' name='brightness' value='" + String(config->brightness) + "' min='0' max='255'></div>";

    // Language selector
    html += "<div><label>Calendar Locale</label>";
    html += "<select name='language'>";
    bool isEn = (strcmp(config->language, "en") == 0 || config->language[0] == '\0');
    bool isCs = (strcmp(config->language, "cs") == 0);
    bool isDe = (strcmp(config->language, "de") == 0);
    html += "<option value='en'" + String(isEn ? " selected" : "") + ">English</option>";
    html += "<option value='cs'" + String(isCs ? " selected" : "") + ">Czech</option>";
    html += "<option value='de'" + String(isDe ? " selected" : "") + ">German</option>";
    html += "</select>";
    html += "<p class='info' style='margin-top:2px; font-size:11px; color:#999;'>Language for day/month names in status bar</p></div>";

    html += "<div style='margin-top:10px;'><label><input type='checkbox' name='debugmode' " + String(config->debugMode ? "checked" : "") + "> Enable Debug Mode (Telnet on port 23)</label></div>";
    html += "<div style='margin-top:10px;'><label><input type='checkbox' name='showplatform' " + String(config->showPlatform ? "checked" : "") + "> Show Platform/Track</label>";
    html += "<p class='info' style='margin-top:2px; font-size:11px; color:#999;'>";
    html += "Display platform between destination and ETA (if available). ";
    html += "Reduces destination space by 2-3 characters.</p></div>";
    html += "<div style='margin-top:10px;'><label><input type='checkbox' name='scrollenabled' " + String(config->scrollEnabled ? "checked" : "") + "> Enable Scrolling</label>";
    html += "<p class='info' style='margin-top:2px; font-size:11px; color:#999;'>";
    html += "Scroll long destination names that don't fit on the display.</p></div>";
    html += "</div>";

    // Weather section (only show when not in AP mode)
    if (!apModeActive)
    {
        html += "<div class='card'>";
        html += "<h2>Weather</h2>";
        html += "<p class='info'>Display 3-hour weather forecast from Open-Meteo (free, no API key required).</p>";

        html += "<div style='margin-top:10px;'><label><input type='checkbox' name='weather_enabled' " + String(config->weatherEnabled ? "checked" : "") + "> Enable Weather Display</label></div>";

        html += "<div style='margin-top:10px;'><label>Latitude (e.g., 50.0755 for Prague)</label>";
        html += "<input type='number' name='weather_lat' step='0.0001' value='" + String(config->weatherLatitude, 4) + "' placeholder='50.0755' style='width:100%;'></div>";

        html += "<div style='margin-top:10px;'><label>Longitude (e.g., 14.4378 for Prague)</label>";
        html += "<input type='number' name='weather_lon' step='0.0001' value='" + String(config->weatherLongitude, 4) + "' placeholder='14.4378' style='width:100%;'></div>";

        html += "<div style='margin-top:10px;'><label>Refresh Interval (minutes, 10-60)</label>";
        html += "<input type='number' name='weather_refresh' min='10' max='60' value='" + String(config->weatherRefreshInterval) + "' style='width:100%;'></div>";

        html += "<p class='info' style='margin-top:10px; font-size:11px; color:#999;'>";
        html += "Find coordinates: <a href='https://www.latlong.net/' target='_blank' style='color:#00d4ff;'>latlong.net</a></p>";
        html += "</div>";
    }

    // Rest Mode section (only show when not in AP mode)
    if (!apModeActive)
    {
        html += "<div class='card'>";
        html += "<h2>Rest Mode</h2>";
        html += "<p class='info'>Configure time periods when display turns off and API polling stops. ";
        html += "Web server remains accessible. Demo mode can be invoked during rest mode.</p>";
        html += "<p class='info' style='font-size:0.9em; color:#888;'>";
        html += "<strong>Half-hour increments:</strong> Times limited to :00 and :30 (e.g., 22:00, 22:30)<br>";
        html += "<strong>Overnight example:</strong> 22:00-06:00 (display off from 10 PM to 6 AM)<br>";
        html += "<strong>Lunch break:</strong> 12:00-13:00 (display off from noon to 1 PM)</p>";

        html += "<table id='restModeTable' style='width:100%; margin-bottom:10px; border-collapse:collapse;'>";
        html += "<thead><tr style='border-bottom:2px solid #444;'>";
        html += "<th style='text-align:left; padding:8px;'>From Time</th>";
        html += "<th style='text-align:left; padding:8px;'>To Time</th>";
        html += "<th style='text-align:center; padding:8px; width:60px;'>Action</th>";
        html += "</tr></thead>";
        html += "<tbody id='restModeRows'>";

        // Parse existing config
        if (config && strlen(config->restModePeriods) > 0)
        {
            char periodsCopy[256];
            strlcpy(periodsCopy, config->restModePeriods, sizeof(periodsCopy));

            char *token = strtok(periodsCopy, ",");
            while (token != nullptr)
            {
                while (*token == ' ')
                    token++;

                char *dashPos = strchr(token, '-');
                if (dashPos)
                {
                    *dashPos = '\0';
                    const char *fromTime = token;
                    const char *toTime = dashPos + 1;

                    // Parse HH:MM format
                    char fromHour[3] = {0}, fromMin[3] = {0};
                    char toHour[3] = {0}, toMin[3] = {0};
                    if (strlen(fromTime) >= 5)
                    {
                        strncpy(fromHour, fromTime, 2);
                        strncpy(fromMin, fromTime + 3, 2);
                    }
                    if (strlen(toTime) >= 5)
                    {
                        strncpy(toHour, toTime, 2);
                        strncpy(toMin, toTime + 3, 2);
                    }

                    html += "<tr>";

                    // From Time dropdowns
                    html += "<td style='padding:8px;'>";
                    html += "<select class='restFromHour' style='padding:5px; margin-right:5px;'>";
                    for (int h = 0; h < 24; h++)
                    {
                        char hour[3];
                        snprintf(hour, sizeof(hour), "%02d", h);
                        html += "<option value='" + String(hour) + "'";
                        if (strcmp(hour, fromHour) == 0)
                            html += " selected";
                        html += ">" + String(hour) + "</option>";
                    }
                    html += "</select>:<select class='restFromMin' style='padding:5px;'>";
                    html += "<option value='00'";
                    if (strcmp(fromMin, "00") == 0)
                        html += " selected";
                    html += ">00</option>";
                    html += "<option value='30'";
                    if (strcmp(fromMin, "30") == 0)
                        html += " selected";
                    html += ">30</option>";
                    html += "</select></td>";

                    // To Time dropdowns
                    html += "<td style='padding:8px;'>";
                    html += "<select class='restToHour' style='padding:5px; margin-right:5px;'>";
                    for (int h = 0; h < 24; h++)
                    {
                        char hour[3];
                        snprintf(hour, sizeof(hour), "%02d", h);
                        html += "<option value='" + String(hour) + "'";
                        if (strcmp(hour, toHour) == 0)
                            html += " selected";
                        html += ">" + String(hour) + "</option>";
                    }
                    html += "</select>:<select class='restToMin' style='padding:5px;'>";
                    html += "<option value='00'";
                    if (strcmp(toMin, "00") == 0)
                        html += " selected";
                    html += ">00</option>";
                    html += "<option value='30'";
                    if (strcmp(toMin, "30") == 0)
                        html += " selected";
                    html += ">30</option>";
                    html += "</select></td>";

                    // Delete button
                    html += "<td style='padding:8px; text-align:center;'>";
                    html += "<button type='button' onclick='deleteRestRow(this)' style='background:#ff6b6b; color:#fff; padding:5px 10px; border:none; cursor:pointer;'>X</button>";
                    html += "</td>";
                    html += "</tr>";
                }
                token = strtok(nullptr, ",");
            }
        }

        html += "</tbody></table>";
        html += "<button type='button' onclick='addRestRow()' style='background:#00d4ff; color:#fff; padding:8px 15px; border:none; cursor:pointer; margin-bottom:10px;'>+ Add Rest Period</button>";
        html += "<input type='hidden' name='restperiods' id='restPeriodsData' value=''>";
        html += "</div>";
    }

    // Line Colors section (only show when not in AP mode)
    if (!apModeActive)
    {
        html += "<div class='card'>";
        html += "<h2>Line Colors</h2>";
        html += "<p class='info'>Configure custom colors for specific transit lines. Leave empty to use defaults.</p>";
        html += "<p class='info' style='font-size:0.9em; color:#888;'>";
        html += "<strong>Pattern matching:</strong> Use * as position placeholders<br>";
        html += "* <code>9*</code> = 2-digit lines (91-99)<br>";
        html += "* <code>95*</code> = 3-digit lines (950-959)<br>";
        html += "* <code>4**</code> = 3-digit lines (400-499)<br>";
        html += "* <code>C***</code> = 4-digit lines (C000-C999)<br>";
        html += "* Exact matches (e.g., \"A\", \"91\") take priority over patterns";
        html += "</p>";

        // Table header
        html += "<table id='lineColorTable' style='width:100%; margin-bottom:10px; border-collapse: collapse;'>";
        html += "<thead><tr style='border-bottom: 2px solid #444;'>";
        html += "<th style='text-align:left; padding:8px;'>Line</th>";
        html += "<th style='text-align:left; padding:8px;'>Color</th>";
        html += "<th style='text-align:center; padding:8px; width:60px;'>Action</th>";
        html += "</tr></thead>";
        html += "<tbody id='lineColorRows'>";

        // Parse existing config and create rows
        if (config && strlen(config->lineColorMap) > 0)
        {
            char mapCopy[256];
            strlcpy(mapCopy, config->lineColorMap, sizeof(mapCopy));

            char *token = strtok(mapCopy, ",");
            while (token != nullptr)
            {
                char *equals = strchr(token, '=');
                if (equals)
                {
                    *equals = '\0';
                    const char *lineName = token;
                    const char *colorName = equals + 1;

                    // Generate row with values
                    html += "<tr>";
                    html += "<td style='padding:8px;'>";
                    html += "<input type='text' class='lineInput' value='" + String(lineName) + "' ";
                    html += "style='width:80px; padding:5px;' maxlength='5' placeholder='A or 9*'>";
                    html += "</td>";
                    html += "<td style='padding:8px;'>";
                    html += "<select class='colorSelect' style='width:100%; padding:5px;'>";

                    // Color options (mark selected)
                    const char *colors[] = {"RED", "GREEN", "BLUE", "YELLOW", "ORANGE", "PURPLE", "CYAN", "WHITE"};
                    for (int i = 0; i < 8; i++)
                    {
                        html += "<option value='" + String(colors[i]) + "'";
                        if (strcasecmp(colorName, colors[i]) == 0)
                        {
                            html += " selected";
                        }
                        html += ">" + String(colors[i]) + "</option>";
                    }

                    html += "</select>";
                    html += "</td>";
                    html += "<td style='padding:8px; text-align:center;'>";
                    html += "<button type='button' onclick='deleteLineRow(this)' ";
                    html += "style='background:#ff6b6b; color:#fff; padding:5px 10px; border:none; cursor:pointer;'>X</button>";
                    html += "</td>";
                    html += "</tr>";
                }
                token = strtok(nullptr, ",");
            }
        }

        html += "</tbody>";
        html += "</table>";

        // Add Row button
        html += "<button type='button' onclick='addLineRow()' ";
        html += "style='background:#00d4ff; color:#fff; padding:8px 15px; border:none; cursor:pointer; margin-bottom:10px;'>";
        html += "+ Add Line";
        html += "</button>";

        // Hidden input to store serialized data
        html += "<input type='hidden' name='linecolormap' id='lineColorMapData' value=''>";

        html += "</div>";
    }

    if (apModeActive)
    {
        html += "<button type='submit'>Save & Connect to WiFi</button>";
    }
    else
    {
        html += "<button type='submit'>Save Configuration</button>";
    }
    html += "</form>";
    html += "</div>";

    // Actions
    if (!apModeActive)
    {
        html += "<div class='card'>";
        html += "<h2>Actions</h2>";
        html += "<form method='POST' action='/refresh' style='display:inline'>";
        html += "<button type='submit'>Refresh Now</button>";
        html += "</form>";
        html += "<form method='GET' action='/demo' style='display:inline; margin-top:10px'>";
        html += "<button type='submit' style='background:#9b59b6;'>Display Demo</button>";
        html += "</form>";
        html += "<form method='GET' action='/update' style='display:inline; margin-top:10px'>";
        html += "<button type='submit'>Install Firmware</button>";
        html += "</form>";
        html += "<form id='checkUpdateForm' onsubmit='checkForUpdate(event); return false;' style='display:inline; margin-top:10px'>";
        html += "<button type='submit' id='checkUpdateBtn'>Check for Updates</button>";
        html += "</form>";
        html += "<form method='POST' action='/reboot' style='display:inline; margin-top:10px'>";
        html += "<button type='submit' class='danger'>Reboot Device</button>";
        html += "</form>";
        html += "<form method='POST' action='/clear-config' onsubmit='return confirm(\"WARNING: This will erase ALL settings and reboot into setup mode. Continue?\");' style='display:inline; margin-top:10px'>";
        html += "<button type='submit' class='danger'>Reset All Settings</button>";
        html += "</form>";
        html += "<div id='updateStatus' style='display:none; margin-top:15px;'></div>";
        html += "</div>";
    }
    else
    {
        // Demo is available in AP mode
        html += "<div class='card'>";
        html += "<h2>Demo</h2>";
        html += "<p>Try out the display with sample departure data before configuring API access.</p>";
        html += "<form method='GET' action='/demo' style='display:inline'>";
        html += "<button type='submit' style='background:#9b59b6;'>View Display Demo</button>";
        html += "</form>";
        html += "</div>";
    }

    // Add JavaScript
    if (!apModeActive)
    {
        html += FPSTR(SCRIPT_GITHUB_UPDATE);
    }
    html += FPSTR(SCRIPT_CITY_SWITCH);
    if (!apModeActive)
    {
        html += FPSTR(SCRIPT_LINE_COLORS);
        html += FPSTR(SCRIPT_REST_MODE);
    }

    html += FPSTR(HTML_FOOTER);
    return html;
}
