#include "DemoPage.h"
#include "WebTemplates.h"
#include "ClientScripts.h"

String buildDemoPage()
{
    String html = FPSTR(HTML_HEADER);
    html += "<h1>Display Demo</h1>";
    html += "<p style='text-align:center; color:#888; margin-top:-10px; margin-bottom:20px;'>Preview and customize the LED display</p>";

    // Demo configuration card
    html += "<div class='card'>";
    html += "<h2>Sample Departures</h2>";
    html += "<p class='info'>Edit the sample data below to preview different line colors, destinations, and ETAs on your LED matrix display.</p>";

    // Demo form
    html += "<form id='demoForm' onsubmit='startDemo(event); return false;'>";

    // Sample departures (3 rows)
    for (int i = 1; i <= 3; i++)
    {
        html += "<div style='border: 1px solid #333; padding: 15px; margin: 10px 0; border-radius: 5px;'>";
        html += "<h3 style='color: #00d4ff; margin-top: 0;'>Departure " + String(i) + "</h3>";
        html += "<div class='grid'>";

        html += "<div><label>Line Number</label>";
        html += "<input type='text' name='line" + String(i) + "' value='" + (i == 1 ? "12" : (i == 2 ? "C" : "S9")) + "' maxlength='7' required></div>";

        html += "<div><label>Destination</label>";
        html += "<input type='text' name='dest" + String(i) + "' value='" +
                String(i == 1 ? "Stvanice" : (i == 2 ? "Nadr. Holesovice" : "Praha-Eden")) +
                "' maxlength='31' required></div>";

        html += "<div><label>ETA (minutes)</label>";
        html += "<input type='number' name='eta" + String(i) + "' value='" + String(i * 2) + "' min='0' max='120' required></div>";

        html += "<div><label>Platform/Track <span style='color:#888; font-size:0.9em;'>(optional)</span></label>";
        html += "<input type='text' name='platform" + String(i) + "' value='" +
                String(i == 1 ? "2" : (i == 2 ? "1" : "")) +
                "' maxlength='3' placeholder='e.g., 2, A, 12'></div>";

        html += "<div style='margin-top:10px;'><label><input type='checkbox' name='ac" + String(i) + "' " +
                String(i == 1 ? "checked" : "") + "> Air Conditioned</label></div>";

        html += "</div></div>";
    }

    html += "<button type='submit' style='background:#9b59b6; margin-top:20px;'>Start Demo</button>";
    html += "</form>";
    html += "</div>";

    // Status card
    html += "<div class='card'>";
    html += "<h2>Demo Status</h2>";
    html += "<div id='demoStatus'>";
    html += "<p style='color:#888;'>Demo not running. Click \"Start Demo\" above to preview on the LED display.</p>";
    html += "</div>";
    html += "<form method='POST' action='/stop-demo' id='stopDemoForm' style='display:none;'>";
    html += "<button type='submit' class='danger'>Stop Demo & Resume Normal Operation</button>";
    html += "</form>";
    html += "</div>";

    // Info card
    html += "<div class='card' style='background: #2e3b4e;'>";
    html += "<h3 style='color: #00d4ff; margin-top: 0;'>About Demo Mode</h3>";
    html += "<ul style='margin: 10px 0; padding-left: 20px; line-height: 1.6;'>";
    html += "<li>Demo mode displays your custom sample data on the LED matrix</li>";
    html += "<li>While demo is running, API polling and automatic time updates are paused</li>";
    html += "<li>You can click \"Start Demo\" repeatedly to test different configurations</li>";
    html += "<li>Stop demo mode or reboot device to resume normal operation</li>";
    html += "<li>Demo is available in both AP mode (setup) and STA mode (connected)</li>";
    html += "</ul>";
    html += "</div>";

    html += "<p><a href='/'>Back to Dashboard</a></p>";

    // Add JavaScript
    html += FPSTR(SCRIPT_DEMO);

    html += FPSTR(HTML_FOOTER);
    return html;
}
