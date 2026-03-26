#include "InfoTextPage.h"
#include "WebTemplates.h"
#include "ClientScripts.h"

String buildInfoTextPage()
{
    String html = FPSTR(HTML_HEADER);

    // Header
    html += "<div class='header'><div class='header-top'>";
    html += "<div class='header-title'><h1>SpojBoard</h1>";
    html += "<div class='header-subtitle'>Infotext Test</div></div></div></div>";

    html += "<div class='content'>";

    // Info banner
    html += "<div class='banner banner-info' style='margin-bottom:24px;'>";
    html += "<span class='status-dot'></span>";
    html += "<div>Inject a custom infotext into the status bar. Normal operation (departures, weather) continues. The text alternates with the date/time row.</div>";
    html += "</div>";

    // Infotext form
    html += "<form id='infoTextForm' onsubmit='setInfoText(event); return false;'>";
    html += "<div class='form-group'>";
    html += "<div class='form-group-title'>Infotext Message</div>";

    html += "<div>";
    html += "<label for='infoText'>TEXT TO DISPLAY</label>";
    html += "<input type='text' id='infoText' name='infoText' maxlength='255' placeholder='e.g., Tram 22: detour via Malostranska' required>";
    html += "</div>";

    html += "<div class='form-actions' style='margin-top:16px;'>";
    html += "<button type='submit' class='btn-primary' style='background:#e67e22;'>Set Infotext</button>";
    html += "</div>";

    html += "</div>"; // End form-group
    html += "</form>";

    // Status section
    html += "<div class='form-group'>";
    html += "<div class='form-group-title'>Status</div>";
    html += "<div id='infoTextStatus'>";
    html += "<p style='color:#999; margin:0;'>Loading...</p>";
    html += "</div>";
    html += "<div id='infoTextActions' style='display:flex; gap:8px; margin-top:16px;'>";
    html += "<button type='button' onclick='clearInfoText()' id='clearBtn' class='danger' style='display:none;'>Clear Infotext</button>";
    html += "<button type='button' onclick='refreshStatus()' class='btn-secondary'>Refresh</button>";
    html += "</div>";
    html += "</div>";

    // Info card
    html += "<div class='card' style='background:#0a0a0a; border:1px solid #333;'>";
    html += "<h3 style='margin-top:0; font-size:14px; color:#999; text-transform:uppercase; letter-spacing:0.5px;'>About Infotext</h3>";
    html += "<ul style='margin:8px 0; padding-left:20px; color:#999; font-size:13px; line-height:1.8;'>";
    html += "<li>Infotext scrolls in the bottom status bar, alternating with the date/time</li>";
    html += "<li>All normal operations continue (departure fetching, weather, display updates)</li>";
    html += "<li>Use <b> /// </b> to separate multiple messages (same as API format)</li>";
    html += "<li>Clear infotext or reboot to return to normal status bar</li>";
    html += "</ul>";
    html += "</div>";

    html += "<div style='text-align:center; margin-top:24px;'>";
    html += "<a href='/' style='color:#67e8f9; text-decoration:none;'>&larr; Back to Dashboard</a>";
    html += "</div>";

    html += "</div>"; // End content

    // JavaScript
    html += FPSTR(SCRIPT_INFOTEXT);

    html += FPSTR(HTML_FOOTER);
    return html;
}
