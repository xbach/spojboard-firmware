#include "DemoPage.h"
#include "WebTemplates.h"
#include "ClientScripts.h"

String buildDemoPage()
{
    String html = FPSTR(HTML_HEADER);

    // Header
    html += "<div class='header'><div class='header-top'>";
    html += "<div class='header-title'><h1>SpojBoard</h1>";
    html += "<div class='header-subtitle'>Display Demo</div></div></div></div>";

    html += "<div class='content'>";

    // Info banner
    html += "<div class='banner banner-info' style='margin-bottom:24px;'>";
    html += "<span class='status-dot'></span>";
    html += "<div>Edit sample data below to preview different line colors, destinations, and ETAs on your LED matrix display</div>";
    html += "</div>";

    // Demo form
    html += "<form id='demoForm' onsubmit='startDemo(event); return false;'>";

    // Sample departures (3 rows)
    for (int i = 1; i <= 3; i++)
    {
        html += "<div class='form-group'>";
        html += "<div class='form-group-title'>Departure " + String(i) + "</div>";
        html += "<div class='grid'>";

        html += "<div>";
        html += "<label for='line" + String(i) + "'>LINE NUMBER</label>";
        html += "<input type='text' id='line" + String(i) + "' name='line" + String(i) + "' value='" + (i == 1 ? "12" : (i == 2 ? "C" : "S9")) + "' maxlength='7' required>";
        html += "</div>";

        html += "<div>";
        html += "<label for='dest" + String(i) + "'>DESTINATION</label>";
        html += "<input type='text' id='dest" + String(i) + "' name='dest" + String(i) + "' value='" +
                String(i == 1 ? "Stvanice" : (i == 2 ? "Nadr. Holesovice" : "Praha-Eden")) +
                "' maxlength='31' required>";
        html += "</div>";

        html += "<div>";
        html += "<label for='eta" + String(i) + "'>ETA (minutes)</label>";
        html += "<input type='number' id='eta" + String(i) + "' name='eta" + String(i) + "' value='" + String(i * 2) + "' min='0' max='120' required>";
        html += "</div>";

        html += "<div>";
        html += "<label for='eta2_" + String(i) + "'>2ND ETA (optional)</label>";
        html += "<input type='number' id='eta2_" + String(i) + "' name='eta2_" + String(i) + "' value='" +
                String(i == 1 ? "14" : (i == 2 ? "22" : "")) +
                "' min='0' max='120' placeholder='empty = none'>";
        html += "</div>";

        html += "<div>";
        html += "<label for='platform" + String(i) + "'>PLATFORM/TRACK (optional)</label>";
        html += "<input type='text' id='platform" + String(i) + "' name='platform" + String(i) + "' value='" +
                String(i == 1 ? "2" : (i == 2 ? "1" : "")) +
                "' maxlength='3' placeholder='e.g., 2, A, 12'>";
        html += "</div>";

        html += "</div>"; // End grid

        html += "<div style='margin-top:12px;'>";
        html += "<label><input type='checkbox' name='ac" + String(i) + "' " +
                String(i == 1 ? "checked" : "") + "> Air Conditioned</label>";
        html += "</div>";

        html += "</div>"; // End form-group
    }

    html += "<div class='form-actions'>";
    html += "<button type='submit' class='btn-primary' style='background:#9b59b6;'>▶ Start Demo</button>";
    html += "</div>";

    html += "</form>";

    // Status section
    html += "<div class='form-group'>";
    html += "<div class='form-group-title'>Demo Status</div>";
    html += "<div id='demoStatus'>";
    html += "<p style='color:#999; margin:0;'>Demo not running. Click \"Start Demo\" above to preview on the LED display.</p>";
    html += "</div>";
    html += "<form method='POST' action='/stop-demo' id='stopDemoForm' style='display:none; margin-top:16px;'>";
    html += "<button type='submit' class='danger'>⏹ Stop Demo & Resume Normal Operation</button>";
    html += "</form>";
    html += "</div>";

    // Info card
    html += "<div class='card' style='background:#0a0a0a; border:1px solid #333;'>";
    html += "<h3 style='margin-top:0; font-size:14px; color:#999; text-transform:uppercase; letter-spacing:0.5px;'>About Demo Mode</h3>";
    html += "<ul style='margin:8px 0; padding-left:20px; color:#999; font-size:13px; line-height:1.8;'>";
    html += "<li>Demo mode displays your custom sample data on the LED matrix</li>";
    html += "<li>While demo is running, API polling and automatic time updates are paused</li>";
    html += "<li>You can click \"Start Demo\" repeatedly to test different configurations</li>";
    html += "<li>Stop demo mode or reboot device to resume normal operation</li>";
    html += "<li>Demo is available in both AP mode (setup) and STA mode (connected)</li>";
    html += "</ul>";
    html += "</div>";

    html += "<div style='text-align:center; margin-top:24px;'>";
    html += "<a href='/' style='color:#67e8f9; text-decoration:none;'>← Back to Dashboard</a>";
    html += "</div>";

    html += "</div>"; // End content

    // Add JavaScript
    html += FPSTR(SCRIPT_DEMO);

    html += FPSTR(HTML_FOOTER);
    return html;
}
