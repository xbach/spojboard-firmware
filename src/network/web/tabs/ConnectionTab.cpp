#include "ConnectionTab.h"

String buildConnectionTab(const Config* config, bool apModeActive)
{
    String html = "";
    html += "<div id='tab-connection' class='tab-content active'>";

    // WiFi Configuration
    html += "<div class='form-group'>";
    html += "<div class='form-group-title'>WiFi Configuration</div>";

    html += "<label for='ssid'>WIFI SSID</label>";
    html += "<input type='text' id='ssid' name='ssid' value='";
    html += config->wifiSsid;
    html += "' required placeholder='Your WiFi network name'>";

    html += "<label for='password'>WIFI PASSWORD</label>";
    html += "<input type='password' id='password' name='password' placeholder='";
    html += apModeActive ? "Enter WiFi password" : "Leave empty to keep current password";
    html += "'>";

    html += "</div>"; // End WiFi form-group

    // Data Source Selection
    html += "<div class='form-group'>";
    html += "<div class='form-group-title'>Data Source</div>";

    html += "<label for='city'>TRANSIT PROVIDER</label>";
    html += "<select id='city' name='city' required>";

    bool isMqtt = (strcmp(config->city, "MQTT") == 0);
    bool isBerlin = (strcmp(config->city, "Berlin") == 0);
    bool isPrague = !isMqtt && !isBerlin;

    html += "<option value='Prague'";
    if (isPrague)
        html += " selected";
    html += ">Prague (PID/Golemio)</option>";

    html += "<option value='Berlin'";
    if (isBerlin)
        html += " selected";
    html += ">Berlin (BVG)</option>";

    html += "<option value='MQTT'";
    if (isMqtt)
        html += " selected";
    html += ">MQTT (Home Assistant)</option>";

    html += "</select>";
    html += "<div class='help-text'>Select your transit network. Device will restart after changing city.</div>";

    html += "</div>"; // End data source form-group

    html += "</div>"; // End tab-connection

    return html;
}
