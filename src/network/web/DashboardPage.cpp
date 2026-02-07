#include "DashboardPage.h"
#include "TabBuilder.h"
#include "WebTemplates.h"
#include "ClientScripts.h"
#include "tabs/ConnectionTab.h"
#include "tabs/TransitDataTab.h"
#include "tabs/DisplayTab.h"
#include "tabs/OptionalTab.h"
#include "tabs/SystemTab.h"
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
    const char* stopName,
    bool demoModeActive,
    bool restModeActive,
    bool restModeManual)
{
    String html = FPSTR(HTML_HEADER);

    // Header with action buttons
    html += buildHeader(apModeActive, restModeActive, restModeManual);

    // Status banners
    html += buildStatusBanner(apModeActive, demoModeActive, restModeActive, restModeManual, apiError, apiErrorMsg,
                              apSSID, "");

    // Tab navigation
    html += buildTabBar(apModeActive);

    // Configuration form wrapping all tabs
    html += "<form method='POST' action='/save' id='configForm'>";

    // Tab 1: Connection
    html += buildConnectionTab(config, apModeActive);

    // Tab 2: Transit Data
    html += buildTransitDataTab(config);

    // Tab 3: Display
    html += buildDisplayTab(config);

    // Tab 4: Optional Features (STA mode only)
    if (!apModeActive)
    {
        html += buildOptionalTab(config);
    }

    // Submit button
    html += "<div class='form-actions'>";
    if (apModeActive)
    {
        html += "<button type='submit' class='btn-primary'>💾 Save & Connect to WiFi</button>";
    }
    else
    {
        html += "<button type='submit' class='btn-primary'>💾 Save Configuration</button>";
    }
    html += "</div>";

    html += "</form>"; // End config form

    // Tab 5: System (outside form - informational only)
    if (!apModeActive)
    {
        html += buildSystemTab(config, apModeActive, ESP.getFreeHeap(), stopName, departureCount);
    }

    // JavaScript scripts
    html += FPSTR(SCRIPT_TAB_NAVIGATION);
    html += FPSTR(SCRIPT_CITY_SWITCH);
    html += FPSTR(SCRIPT_DISPLAY_TAB);
    html += FPSTR(SCRIPT_CONFIG_SAVE);  // Form save with inline feedback

    if (!apModeActive)
    {
        html += FPSTR(SCRIPT_OPTIONAL_TAB);
        html += FPSTR(SCRIPT_LINE_COLORS);
        html += FPSTR(SCRIPT_REST_MODE);
        html += FPSTR(SCRIPT_REST_MODE_TOGGLE);
        html += FPSTR(SCRIPT_SYSTEM_ACTIONS);
        html += FPSTR(SCRIPT_GITHUB_UPDATE);
    }

    html += FPSTR(HTML_FOOTER);
    return html;
}
