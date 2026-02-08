#include "SystemTab.h"
#include "../WebUtils.h"
#include <WiFi.h>

String buildSystemTab(const Config* config, bool apModeActive, size_t freeHeap, const char* stopName,
                      int departureCount)
{
    String html = "";
    html += "<div id='tab-system' class='tab-content'>";

    // System Information
    html += "<div class='form-group'>";
    html += "<div class='form-group-title'>System Information</div>";

    // Hardware variant
    html += "<div class='info-row'>";
    html += "<span class='info-label'>Hardware:</span>";
    html += "<span class='info-value'>" + String(VARIANT_DISPLAY_NAME) + "</span>";
    html += "</div>";

    // Firmware version with build ID
    char buildIdStr[10];
    snprintf(buildIdStr, sizeof(buildIdStr), "%08x", BUILD_ID);
    html += "<div class='info-row'>";
    html += "<span class='info-label'>Firmware:</span>";
    html += "<span class='info-value'>Release " + String(FIRMWARE_RELEASE) + " (" + String(buildIdStr) + ")</span>";
    html += "</div>";

    // WiFi status
    if (!apModeActive)
    {
        html += "<div class='info-row'>";
        html += "<span class='info-label'>WiFi:</span>";
        html += "<span class='info-value'>" + WiFi.localIP().toString() + "</span>";
        html += "</div>";

        // Transit provider
        html += "<div class='info-row'>";
        html += "<span class='info-label'>Transit Provider:</span>";
        html += "<span class='info-value'>" + escapeHtml(config->city) + "</span>";
        html += "</div>";

        // Current stop name
        if (stopName && stopName[0] != '\0')
        {
            html += "<div class='info-row'>";
            html += "<span class='info-label'>Stop:</span>";
            html += "<span class='info-value'>" + escapeHtml(stopName) + "</span>";
            html += "</div>";
        }

        // Departure count
        html += "<div class='info-row'>";
        html += "<span class='info-label'>Cached Departures:</span>";
        html += "<span class='info-value'>" + String(departureCount) + "</span>";
        html += "</div>";
    }

    // Free memory
    html += "<div class='info-row'>";
    html += "<span class='info-label'>Free Memory:</span>";
    html += "<span class='info-value'>" + String(freeHeap) + " bytes</span>";
    html += "</div>";

    html += "</div>"; // End system info form-group

    // Firmware Updates (STA mode only)
    if (!apModeActive)
    {
        html += "<div class='form-group'>";
        html += "<div class='form-group-title'>Firmware Updates</div>";

        html += "<div>";
        html += "<button type='button' class='secondary' onclick='checkForUpdate(event)' id='checkUpdateBtn'>Check for "
                "Updates</button>";
        html += "<div class='help-text'>Check GitHub for new firmware releases</div>";
        html += "</div>";

        html += "<div id='updateStatus' style='margin-top:10px;'></div>";

        html += "<div style='margin-top:15px;'>";
        html += "<button type='button' class='secondary' onclick=\"window.location.href='/update'\">Manual Firmware "
                "Upload</button>";
        html += "<div class='help-text'>Upload .bin file directly from your computer</div>";
        html += "</div>";

        html += "</div>"; // End firmware updates form-group
    }

    // System Actions
    html += "<div class='form-group'>";
    html += "<div class='form-group-title'>System Actions</div>";

    // Reboot button
    html += "<div>";
    html += "<button type='button' class='danger' onclick='rebootDevice()'>Reboot Device</button>";
    html += "<div class='help-text'>Restart the device (takes ~10 seconds)</div>";
    html += "</div>";

    // Factory reset button
    html += "<div style='margin-top:15px;'>";
    html += "<button type='button' class='danger' onclick='factoryReset()'>Factory Reset</button>";
    html += "<div class='help-text'>⚠ Erase all settings and return to setup mode</div>";
    html += "</div>";

    html += "</div>"; // End system actions form-group

    html += "</div>"; // End tab-system

    return html;
}
