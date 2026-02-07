#include "TransitDataTab.h"

// Helper: Build Prague API section
static String buildPragueSection(const Config* config, bool isPrague)
{
    String html = "";
    html += "<div id='pragueSection' class='form-group'";
    if (!isPrague)
    {
        html += " style='display:none;'";
    }
    html += ">";
    html += "<div class='form-group-title'>Prague (Golemio API)</div>";

    // API Key field
    html += "<label for='pragueApiKey'>API KEY</label>";
    html += "<input type='password' id='pragueApiKey' name='api_key' placeholder='";
    html += (strlen(config->pragueApiKey) > 0) ? "Leave empty to keep current API key" : "Enter Golemio API key";
    html += "'>";
    html += "<div class='help-text'>Get your free API key at <a href='https://api.golemio.cz/api-keys' "
            "target='_blank' style='color:#67e8f9;'>api.golemio.cz</a></div>";

    // Stop IDs field
    html += "<label for='pragueStopIds'>STOP IDS</label>";
    html += "<input type='text' id='pragueStopIds' name='prague_stops' value='";
    html += config->pragueStopIds;
    html += "' placeholder='U693Z2P,U694Z2P'>";
    html += "<div class='help-text'>Comma-separated GTFS stop IDs. Find IDs at <a "
            "href='https://data.pid.cz/stops/json/stops.json' target='_blank' "
            "style='color:#67e8f9;'>PID data portal</a></div>";

    html += "</div>"; // End pragueSection
    return html;
}

// Helper: Build Berlin API section
static String buildBerlinSection(const Config* config, bool isBerlin)
{
    String html = "";
    html += "<div id='berlinSection' class='form-group'";
    if (!isBerlin)
    {
        html += " style='display:none;'";
    }
    html += ">";
    html += "<div class='form-group-title'>Berlin (BVG API)</div>";

    // Stop IDs field (no API key needed)
    html += "<label for='berlinStopIds'>STOP IDS</label>";
    html += "<input type='text' id='berlinStopIds' name='berlin_stops' value='";
    html += config->berlinStopIds;
    html += "' placeholder='900013102,900014101'>";
    html += "<div class='help-text'>Comma-separated numeric stop IDs. Find IDs at <a "
            "href='https://v6.bvg.transport.rest/' target='_blank' "
            "style='color:#67e8f9;'>BVG REST API</a> (use /locations endpoint)</div>";

    html += "</div>"; // End berlinSection
    return html;
}

// Helper: Build MQTT section
static String buildMqttSection(const Config* config, bool isMqtt)
{
    String html = "";
    html += "<div id='mqttSection' class='form-group'";
    if (!isMqtt)
    {
        html += " style='display:none;'";
    }
    html += ">";
    html += "<div class='form-group-title'>MQTT Configuration</div>";

    // Broker address
    html += "<label for='mqttBroker'>BROKER ADDRESS</label>";
    html += "<input type='text' id='mqttBroker' name='mqtt_broker' value='";
    html += config->mqttBroker;
    html += "' placeholder='homeassistant.local or 192.168.1.100'>";

    // Port + Username (2-column grid)
    html += "<div class='grid'>";
    html += "<div>";
    html += "<label for='mqttPort'>PORT</label>";
    html += "<input type='number' id='mqttPort' name='mqtt_port' value='";
    html += String(config->mqttPort);
    html += "' min='1' max='65535'>";
    html += "</div>";
    html += "<div>";
    html += "<label for='mqttUsername'>USERNAME (optional)</label>";
    html += "<input type='text' id='mqttUsername' name='mqtt_user' value='";
    html += config->mqttUsername;
    html += "' placeholder='Leave empty for no auth'>";
    html += "</div>";
    html += "</div>";

    // Password
    html += "<label for='mqttPassword'>PASSWORD (optional)</label>";
    html += "<input type='password' id='mqttPassword' name='mqtt_pass' placeholder='";
    html += (strlen(config->mqttPassword) > 0) ? "Leave empty to keep current password" : "Leave empty for no auth";
    html += "'>";

    // Request + Response Topics (2-column grid)
    html += "<div class='grid'>";
    html += "<div>";
    html += "<label for='mqttRequestTopic'>REQUEST TOPIC</label>";
    html += "<input type='text' id='mqttRequestTopic' name='mqtt_req_topic' value='";
    html += config->mqttRequestTopic;
    html += "' placeholder='spojboard/request'>";
    html += "</div>";
    html += "<div>";
    html += "<label for='mqttResponseTopic'>RESPONSE TOPIC</label>";
    html += "<input type='text' id='mqttResponseTopic' name='mqtt_resp_topic' value='";
    html += config->mqttResponseTopic;
    html += "' placeholder='spojboard/response'>";
    html += "</div>";
    html += "</div>";

    // ETA Mode
    html += "<label for='mqttUseEtaMode'>ETA MODE</label>";
    html += "<select id='mqttUseEtaMode' name='mqtt_eta_mode'>";
    html += "<option value='0'";
    if (!config->mqttUseEtaMode)
        html += " selected";
    html += ">Timestamp Mode (server sends unix timestamps, device recalculates ETAs)</option>";
    html += "<option value='1'";
    if (config->mqttUseEtaMode)
        html += " selected";
    html += ">ETA Mode (server sends pre-calculated minutes)</option>";
    html += "</select>";
    html += "<div class='help-text'>Choose how departure times are provided by your MQTT source</div>";

    // JSON Field Mappings subsection
    html += "<div class='form-group-title' style='margin-top:24px;'>JSON Field Mappings</div>";
    html += "<div class='help-text' style='margin-bottom:12px;'>Customize JSON field names used in MQTT response "
            "messages</div>";
    html += "<div class='grid'>";

    // Line Number Field + Destination Field
    html += "<div>";
    html += "<label for='mqttFieldLine'>LINE NUMBER FIELD</label>";
    html += "<input type='text' id='mqttFieldLine' name='mqtt_fld_line' value='";
    html += config->mqttFieldLine;
    html += "' placeholder='line'>";
    html += "</div>";
    html += "<div>";
    html += "<label for='mqttFieldDestination'>DESTINATION FIELD</label>";
    html += "<input type='text' id='mqttFieldDestination' name='mqtt_fld_dest' value='";
    html += config->mqttFieldDestination;
    html += "' placeholder='dest'>";
    html += "</div>";

    // ETA Field + Timestamp Field
    html += "<div>";
    html += "<label for='mqttFieldEta'>ETA FIELD (minutes)</label>";
    html += "<input type='text' id='mqttFieldEta' name='mqtt_fld_eta' value='";
    html += config->mqttFieldEta;
    html += "' placeholder='eta'>";
    html += "</div>";
    html += "<div>";
    html += "<label for='mqttFieldTimestamp'>TIMESTAMP FIELD (unix)</label>";
    html += "<input type='text' id='mqttFieldTimestamp' name='mqtt_fld_time' value='";
    html += config->mqttFieldTimestamp;
    html += "' placeholder='dep'>";
    html += "</div>";

    // Platform Field + AC Flag Field
    html += "<div>";
    html += "<label for='mqttFieldPlatform'>PLATFORM FIELD (optional)</label>";
    html += "<input type='text' id='mqttFieldPlatform' name='mqtt_fld_plat' value='";
    html += config->mqttFieldPlatform;
    html += "' placeholder='plt'>";
    html += "</div>";
    html += "<div>";
    html += "<label for='mqttFieldAC'>AC FLAG FIELD (optional)</label>";
    html += "<input type='text' id='mqttFieldAC' name='mqtt_fld_ac' value='";
    html += config->mqttFieldAC;
    html += "' placeholder='ac'>";
    html += "</div>";

    html += "</div>"; // end grid
    html += "</div>"; // end mqttSection
    return html;
}

// Helper: Build common settings (always visible for all cities)
static String buildCommonSettings(const Config* config, bool isMqtt)
{
    String html = "";
    html += "<div class='form-group'>";
    html += "<div class='form-group-title'>Common Settings</div>";
    html += "<div class='grid'>";

    // Refresh interval (NOT shown for MQTT)
    if (!isMqtt)
    {
        html += "<div>";
        html += "<label for='refreshInterval'>API REFRESH INTERVAL (seconds)</label>";
        html += "<input type='number' id='refreshInterval' name='refresh' value='";
        html += String(config->refreshInterval);
        html += "' min='10' max='300'>";
        html += "<div class='help-text'>How often to fetch departure data (default: 60)</div>";
        html += "</div>";
    }

    // Min departure time (shown for ALL cities)
    html += "<div>";
    html += "<label for='minDepartureTime'>MIN DEPARTURE TIME (minutes)</label>";
    html += "<input type='number' id='minDepartureTime' name='min_dep_time' value='";
    html += String(config->minDepartureTime);
    html += "' min='0' max='30'>";
    html += "<div class='help-text'>Filter out departures leaving in less than this time (default: 3)</div>";
    html += "</div>";

    html += "</div>"; // end grid
    html += "</div>"; // end form-group
    return html;
}

// Main builder
String buildTransitDataTab(const Config* config)
{
    String html = "";
    html += "<div id='tab-transit' class='tab-content'>";

    // Determine which section should be visible
    bool isMqtt = (strcmp(config->city, "MQTT") == 0);
    bool isBerlin = (strcmp(config->city, "Berlin") == 0);
    bool isPrague = !isMqtt && !isBerlin;

    // City-specific API sections (with visibility control)
    html += buildPragueSection(config, isPrague);
    html += buildBerlinSection(config, isBerlin);
    html += buildMqttSection(config, isMqtt);

    // Common settings (refresh interval + min departure time)
    html += buildCommonSettings(config, isMqtt);

    html += "</div>"; // End tab-transit
    return html;
}
