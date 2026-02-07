#include "OptionalTab.h"

String buildOptionalTab(const Config* config)
{
    String html = "";
    html += "<div id='tab-optional' class='tab-content'>";

    // Weather Configuration
    html += "<div class='form-group'>";
    html += "<div class='form-group-title'>Weather Configuration</div>";

    // Weather enabled checkbox
    html += "<div>";
    html += "<label><input type='checkbox' id='weatherEnabled' name='weather_enabled'";
    if (config->weatherEnabled)
        html += " checked";
    html += "> Enable weather display</label>";
    html += "<div class='help-text'>Show temperature and weather icon in status bar</div>";
    html += "</div>";

    // Weather location (2-column grid)
    html += "<div class='grid' id='weatherSettings' style='";
    if (!config->weatherEnabled)
        html += "display:none;";
    html += "'>";

    // Latitude
    html += "<div>";
    html += "<label for='weatherLatitude'>LATITUDE</label>";
    html += "<input type='number' id='weatherLatitude' name='weather_lat' step='0.0001' min='-90' max='90' "
            "value='";
    html += String(config->weatherLatitude, 4);
    html += "'>";
    html += "<div class='help-text'>GPS latitude (e.g., 50.0755 for Prague)</div>";
    html += "</div>";

    // Longitude
    html += "<div>";
    html += "<label for='weatherLongitude'>LONGITUDE</label>";
    html += "<input type='number' id='weatherLongitude' name='weather_lon' step='0.0001' min='-180' max='180' "
            "value='";
    html += String(config->weatherLongitude, 4);
    html += "'>";
    html += "<div class='help-text'>GPS longitude (e.g., 14.4378 for Prague)</div>";
    html += "</div>";

    html += "</div>"; // End weather location grid

    // Weather refresh interval
    html += "<div id='weatherRefresh' style='";
    if (!config->weatherEnabled)
        html += "display:none;";
    html += "'>";
    html += "<label for='weatherRefreshInterval'>REFRESH INTERVAL (minutes)</label>";
    html += "<input type='number' id='weatherRefreshInterval' name='weather_refresh' min='5' max='120' value='";
    html += String(config->weatherRefreshInterval);
    html += "'>";
    html += "<div class='help-text'>How often to fetch weather data (default: 15)</div>";
    html += "</div>";

    html += "</div>"; // End weather form-group

    // Rest Mode Configuration
    html += "<div class='form-group'>";
    html += "<div class='form-group-title'>Rest Mode</div>";

    html += "<div>";
    html += "<label for='restModePeriods'>TIME PERIODS</label>";
    html += "<input type='text' id='restModePeriods' name='rest_periods' value='";
    html += config->restModePeriods;
    html += "' placeholder='HH:MM-HH:MM,HH:MM-HH:MM'>";
    html += "<div class='help-text'>Display turns off during these times (e.g., \"23:00-07:00\"). Multiple "
            "periods comma-separated.</div>";
    html += "</div>";

    html += "</div>"; // End rest mode form-group

    // Debug Mode
    html += "<div class='form-group'>";
    html += "<div class='form-group-title'>Debug</div>";

    html += "<div>";
    html += "<label><input type='checkbox' name='debug_mode'";
    if (config->debugMode)
        html += " checked";
    html += "> Enable debug mode</label>";
    html += "<div class='help-text'>Enable telnet logging and verbose output</div>";
    html += "</div>";

    html += "</div>"; // End debug form-group

    html += "</div>"; // End tab-optional

    return html;
}
