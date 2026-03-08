#include "DisplayController.h"
#include "DisplayColors.h"

DisplayController::DisplayController(DisplayManager& dm)
    : displayManager(dm)
{
}

void DisplayController::render(const Departure* departures, int departureCount, int numToDisplay,
                               bool wifiConnected, bool apModeActive,
                               const char* apSSID, const char* apPassword,
                               bool apiError, const char* apiErrorMsg,
                               const char* stopName, bool apiKeyConfigured,
                               bool demoModeActive, bool restModeActive,
                               bool departuresLoading,
                               bool tickerModeActive,
                               const TickerData* tickerData)
{
    // ========================================================================
    // State Machine: Determine what to display based on priority
    // ========================================================================

    // Priority 1: Demo mode (overrides everything)
    if (demoModeActive)
    {
        // Demo mode shows custom departures, handled by caller
        // Controller just passes through to normal departure rendering
        displayManager.drawDepartures(departures, departureCount, numToDisplay);
        return;
    }

    // Priority 2: Rest mode (display off)
    if (restModeActive)
    {
        // Display is turned off, but we still need to clear it
        displayManager.clearDisplay();
        return;
    }

    // Priority 3: Ticker mode (candlestick chart)
    if (tickerModeActive)
    {
        if (tickerData && tickerData->valid)
        {
            displayManager.drawTicker(*tickerData);
        }
        else
        {
            const char* sym = (tickerData && tickerData->symbol[0]) ? tickerData->symbol : "";
            displayManager.drawStatus("Loading Ticker...", sym, COLOR_YELLOW);
            displayManager.drawDateTime();
        }
        return;
    }

    // Priority 4: AP mode (show setup credentials)
    if (apModeActive)
    {
        displayManager.drawAPMode(apSSID, apPassword);
        return;
    }

    // Priority 4: WiFi connecting
    if (!wifiConnected)
    {
        displayManager.drawStatus("WiFi Connecting...", "", COLOR_YELLOW);
        return;
    }

    // Priority 5: Setup required (no API key)
    if (!apiKeyConfigured)
    {
        char ipStr[32];
        sprintf(ipStr, "http://%s", WiFi.localIP().toString().c_str());
        displayManager.drawStatus("Setup Required", ipStr, COLOR_CYAN);
        return;
    }

    // Priority 6: API error
    if (apiError)
    {
        // Show IP address so user can access web UI to fix config
        char ipStr[32];
        sprintf(ipStr, "Fix at: %s", WiFi.localIP().toString().c_str());
        displayManager.drawStatus("API Error", ipStr, COLOR_RED);
        displayManager.drawDateTime(); // Still show time bar
        return;
    }

    // Priority 7: No departures (loading or genuinely empty)
    if (departureCount == 0)
    {
        if (departuresLoading)
        {
            displayManager.drawStatus("Loading Departures...", "", COLOR_YELLOW);
        }
        else
        {
            displayManager.drawStatus("No Departures", "", COLOR_YELLOW);
        }
        displayManager.drawDateTime();
        return;
    }

    // Priority 8: Normal operation - show departures
    displayManager.drawDepartures(departures, departureCount, numToDisplay);
}
