#include "TabBuilder.h"

String buildHeader(bool apModeActive, bool restModeActive, bool restModeManual)
{
    String html = "<div class=\"header\">";
    html += "<div class=\"header-top\">";

    // Title section
    html += "<div class=\"header-title\">";
    html += "<h1>SpojBoard</h1>";
    html += "<div class=\"header-subtitle\">Smart Panel for Onward Journeys</div>";
    html += "</div>";

    // Action bar
    html += "<div class=\"action-bar\">";

    if (!apModeActive)
    {
        // Refresh button (STA mode only)
        html += "<button class=\"action-btn\" onclick=\"refreshDepartures()\" title=\"Refresh Now\">↻</button>";

        // Demo button (STA mode)
        html += "<button class=\"action-btn\" onclick=\"window.location.href='/demo'\" title=\"Display Demo\">▶</button>";

        // Rest mode toggle button (STA mode only)
        String restClass = (restModeActive && restModeManual) ? "action-btn active" : "action-btn";
        html += "<button id=\"restModeBtn\" class=\"" + restClass + "\" onclick=\"toggleRestMode()\" title=\"";
        html += (restModeActive && restModeManual) ? "Disable Rest Mode" : "Enable Rest Mode";
        html += "\">💤</button>";
    }
    else
    {
        // Only demo button in AP mode
        html += "<button class=\"action-btn\" onclick=\"window.location.href='/demo'\" title=\"Display Demo\">▶</button>";
    }

    html += "</div>"; // action-bar
    html += "</div>"; // header-top
    html += "</div>"; // header

    return html;
}

String buildTabBar(bool apModeActive)
{
    String html = "<div class=\"tabs\">";

    // Tab 1: Connection (always visible)
    html += "<button class=\"tab active\" data-tab=\"connection\">";
    html += "<span class=\"tab-icon\">📡</span>";
    html += "<span class=\"tab-label\">Connection</span>";
    html += "</button>";

    // Tab 2: Transit Data (always visible)
    html += "<button class=\"tab\" data-tab=\"transit\">";
    html += "<span class=\"tab-icon\">🚇</span>";
    html += "<span class=\"tab-label\">Transit Data</span>";
    html += "</button>";

    // Tab 3: Display (always visible)
    html += "<button class=\"tab\" data-tab=\"display\">";
    html += "<span class=\"tab-icon\">🖥</span>";
    html += "<span class=\"tab-label\">Display</span>";
    html += "</button>";

    if (!apModeActive)
    {
        // Tab 4: Optional Features (STA mode only)
        html += "<button class=\"tab\" data-tab=\"optional\">";
        html += "<span class=\"tab-icon\">⭐</span>";
        html += "<span class=\"tab-label\">Optional</span>";
        html += "</button>";

        // Tab 5: System (STA mode only)
        html += "<button class=\"tab\" data-tab=\"system\">";
        html += "<span class=\"tab-icon\">⚙</span>";
        html += "<span class=\"tab-label\">System</span>";
        html += "</button>";
    }

    html += "</div>"; // tabs

    return html;
}

String buildStatusBanner(bool apModeActive, bool demoModeActive, bool restModeActive, bool restModeManual,
                         bool hasApiError, const char* apiErrorMsg, const char* apSsid, const char* apPassword)
{
    String html = "";

    // AP Mode banner
    if (apModeActive)
    {
        html += "<div class=\"banner banner-warn\">";
        html += "<span class=\"status-dot\"></span>";
        html += "<div><strong>📶 AP Mode Active</strong> - Configure WiFi to connect to your network";
        if (apSsid && apSsid[0])
        {
            html += "<br>Network: <strong>" + String(apSsid) + "</strong>";
            if (apPassword && apPassword[0])
            {
                html += " | Password: <strong>" + String(apPassword) + "</strong>";
            }
        }
        html += "</div>";
        html += "</div>";
    }

    // Demo Mode banner
    if (demoModeActive)
    {
        html += "<div class=\"banner banner-warning\">";
        html += "<span class=\"status-dot\"></span>";
        html += "<div><strong>▶ Demo Mode Active</strong> - Showing sample departures. ";
        html += "<a href=\"/demo\" style=\"color:inherit;text-decoration:underline;\">Configure demo data</a>";
        html += "</div>";
        html += "</div>";
    }

    // Rest Mode banner
    if (restModeActive)
    {
        html += "<div class=\"banner banner-info\">";
        html += "<span class=\"status-dot\"></span>";
        html += "<div><strong>💤 Rest Mode Active";
        if (restModeManual)
        {
            html += " (Manual)";
        }
        else
        {
            html += " (Scheduled)";
        }
        html += "</strong> - Display is off. Display will resume automatically.";
        html += "</div>";
        html += "</div>";
    }

    // API Error banner
    if (hasApiError && apiErrorMsg != nullptr && strlen(apiErrorMsg) > 0)
    {
        html += "<div class=\"banner banner-error\">";
        html += "<span class=\"status-dot\"></span>";
        html += "<div><strong>⚠ API Error:</strong> " + String(apiErrorMsg) + "</div>";
        html += "</div>";
    }

    return html;
}
