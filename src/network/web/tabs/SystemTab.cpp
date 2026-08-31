#include "SystemTab.h"
#include "../WebUtils.h"
#include "../../WiFiManager.h"
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

    // Firmware version with build ID. The ID is a git SHA prefix, so it is a
    // real reference -- unless the tree was dirty, in which case it names a
    // commit this binary does NOT match, and must say so.
    char buildIdStr[24];
    snprintf(buildIdStr, sizeof(buildIdStr), "%08x%s", BUILD_ID, BUILD_DIRTY ? "-dirty" : "");
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

        // Hostname: what this device announces over DHCP, i.e. how it appears in a
        // router's client list. Not resolvable as a name yet -- there is no mDNS
        // responder -- so it is shown bare, without a ".local" suffix that would not
        // actually work.
        html += "<div class='info-row'>";
        html += "<span class='info-label'>Hostname:</span>";
        html += "<span class='info-value'>" + String(WiFiManager::getHostname()) + "</span>";
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

        // Departure count with link to details
        html += "<div class='info-row'>";
        html += "<span class='info-label'>Cached Departures:</span>";
        html += "<span class='info-value'><a href='/departures' style='color:#67e8f9;text-decoration:none;'>"
                + String(departureCount) + " &rarr;</a></span>";
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

    // Configuration Backup (TA-0307). Rendered in BOTH modes: restoring a
    // config and resetting to factory are what a user reaches for when the
    // device has dropped to AP mode, so gating them on STA would put the
    // recovery tools behind the failure they recover from.
    html += "<div class='form-group'>";
    html += "<div class='form-group-title'>Configuration Backup</div>";

    html += "<div>";
    html += "<button type='button' class='secondary' onclick='exportConfig()'>Download Backup</button>";
    // The plaintext warning belongs HERE, next to the button that produces the
    // file -- not in the docs, which the person about to paste it into a GitHub
    // issue will not have read.
    html += "<div class='help-text'>Saves every setting as a JSON file. <strong style='color:#fcd34d;'>Includes your "
            "WiFi password and API keys in plain text</strong> &mdash; treat the file as a secret and do not paste it "
            "into a bug report.</div>";
    html += "</div>";

    html += "<div style='margin-top:15px;'>";
    html += "<label class='form-label' for='cfgFile'>Restore from backup</label>";
    html += "<input type='file' id='cfgFile' accept='.json,application/json'>";

    // Both default OFF. A restore must not be able to scramble a working panel
    // by surprise; the two are separate because they describe different things
    // -- the panels, and the wiring to this particular controller.
    html += "<div style='margin-top:10px;'>";
    html += "<label style='display:flex;align-items:center;gap:8px;font-size:13px;color:#bbb;'>";
    html += "<input type='checkbox' id='cfgGeom'> Also restore panel arrangement</label>";
    html += "<label style='display:flex;align-items:center;gap:8px;font-size:13px;color:#bbb;margin-top:6px;'>";
    html += "<input type='checkbox' id='cfgWiring'> Also restore panel wiring "
            "<span style='color:#666;'>(same board only)</span></label>";
    html += "</div>";

    html += "<div style='margin-top:12px;'>";
    html += "<button type='button' class='secondary' onclick='importConfig()' id='cfgImportBtn'>Restore &amp; "
            "Reboot</button>";
    html += "</div>";
    html += "<div class='help-text'>Settings the file does not contain keep their current value. Nothing is written "
            "unless the whole file is valid.</div>";
    html += "<div id='cfgImportStatus' style='margin-top:10px;font-size:13px;'></div>";
    html += "</div>";

    html += "</div>"; // End configuration backup form-group

    // System Actions
    html += "<div class='form-group'>";
    html += "<div class='form-group-title'>System Actions</div>";

    // Reboot button
    html += "<div>";
    html += "<button type='button' class='danger' onclick='rebootDevice()'>Reboot Device</button>";
    html += "<div class='help-text'>Restart the device (takes ~10 seconds)</div>";
    html += "</div>";

    // Factory reset: type-to-confirm rather than two confirm() dialogs, which
    // are dismissed reflexively. The typed word is also required by the SERVER
    // (POST /clear-config needs confirm=RESET), so the guard is not merely
    // cosmetic -- a stray fetch cannot wipe the device.
    html += "<div style='margin-top:15px;'>";
    html += "<label class='form-label' for='resetConfirm'>Factory Reset</label>";
    html += "<div class='help-text' style='margin-top:0;'>Erases every setting and reboots into setup mode. "
            "<strong style='color:#fb7185;'>Panel arrangement and wiring are erased too</strong> &mdash; if you run "
            "64px panels or custom wiring the display will be wrong until you set it again on the Hardware tab, which "
            "is reachable in setup mode. Download a backup first if you have not, or keep them below.</div>";

    // Opt-ins to KEEP, mirroring the import's opt-ins to RESTORE. Both default
    // off, so the unqualified action is still a full factory reset -- a reset
    // that quietly preserved things would be the more surprising default.
    html += "<div style='margin-top:10px;'>";
    html += "<label style='display:flex;align-items:center;gap:8px;font-size:13px;color:#bbb;'>";
    html += "<input type='checkbox' id='keepWifi'> Keep WiFi credentials "
            "<span style='color:#666;'>(stays on your network instead of starting a hotspot)</span></label>";
    html += "<label style='display:flex;align-items:center;gap:8px;font-size:13px;color:#bbb;margin-top:6px;'>";
    html += "<input type='checkbox' id='keepDisplay'> Keep panel arrangement and wiring "
            "<span style='color:#666;'>(display comes back looking the same)</span></label>";
    html += "</div>";

    html += "<input type='text' id='resetConfirm' placeholder='Type RESET to confirm' autocomplete='off' "
            "oninput='onResetConfirmInput()' style='margin-top:8px;'>";
    html += "<button type='button' class='danger' id='resetBtn' onclick='factoryReset()' disabled "
            "style='margin-top:8px;opacity:0.5;'>Erase Everything</button>";
    html += "</div>";

    html += "</div>"; // End system actions form-group

    html += "</div>"; // End tab-system

    return html;
}
