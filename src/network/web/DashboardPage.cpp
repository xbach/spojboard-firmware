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

// Send a chunk safely: skip empty strings (which would terminate chunked transfer)
// and yield to let the TCP stack flush between chunks.
static void sendChunk(WebServer* server, const String& content)
{
    if (content.length() > 0)
    {
        server->sendContent(content);
        yield();
    }
}

void sendDashboardPage(
    WebServer* server,
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
    // Use chunked transfer encoding to avoid building ~50KB HTML in RAM.
    // Each section is sent and freed before the next is built.
    server->setContentLength(CONTENT_LENGTH_UNKNOWN);
    server->send(200, "text/html", "");

    // Chunk 1: CSS + page structure (~12KB)
    {
        String structure = FPSTR(HTML_HEADER);
        structure += buildHeader(apModeActive, restModeActive, restModeManual);
        structure += buildStatusBanner(apModeActive, demoModeActive, restModeActive, restModeManual, apiError, apiErrorMsg,
                                  apSSID, "");
        structure += buildTabBar(apModeActive);
        structure += "<form method='POST' action='/save' id='configForm'>";
        sendChunk(server, structure);
    }

    // Chunk 2: Connection + Transit Data tabs (~8KB)
    {
        String tabs1 = buildConnectionTab(config, apModeActive);
        tabs1 += buildTransitDataTab(config);
        sendChunk(server, tabs1);
    }

    // Chunk 3: Display tab + Optional tab + submit button (~8KB)
    {
        String tabs2 = buildDisplayTab(config);
        if (!apModeActive)
        {
            tabs2 += buildOptionalTab(config);
        }
        tabs2 += "<div class='form-actions'>";
        if (apModeActive)
        {
            tabs2 += "<button type='submit' class='btn-primary'>💾 Save & Connect to WiFi</button>";
        }
        else
        {
            tabs2 += "<button type='submit' class='btn-primary'>💾 Save Configuration</button>";
        }
        tabs2 += "</div>";
        tabs2 += "</form>";
        sendChunk(server, tabs2);
    }

    // Chunk 4: System tab (STA mode only, ~5KB)
    if (!apModeActive)
    {
        sendChunk(server, buildSystemTab(config, apModeActive, ESP.getFreeHeap(), stopName, departureCount));
    }

    // Chunk 5: Core scripts (~6KB)
    {
        String scripts = FPSTR(SCRIPT_TAB_NAVIGATION);
        scripts += FPSTR(SCRIPT_CITY_SWITCH);
        scripts += FPSTR(SCRIPT_DISPLAY_TAB);
        scripts += FPSTR(SCRIPT_CONFIG_SAVE);
        sendChunk(server, scripts);
    }

    // Chunk 6: Optional scripts (~18KB, STA mode only)
    if (!apModeActive)
    {
        String optScripts = FPSTR(SCRIPT_OPTIONAL_TAB);
        optScripts += FPSTR(SCRIPT_LINE_COLORS);
        optScripts += FPSTR(SCRIPT_REST_MODE);
        optScripts += FPSTR(SCRIPT_REST_MODE_TOGGLE);
        optScripts += FPSTR(SCRIPT_SYSTEM_ACTIONS);
        optScripts += FPSTR(SCRIPT_GITHUB_UPDATE);
        sendChunk(server, optScripts);
    }

    sendChunk(server, FPSTR(HTML_FOOTER));

    // Terminate chunked transfer
    server->sendContent("");
}
