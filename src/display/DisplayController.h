#ifndef DISPLAYCONTROLLER_H
#define DISPLAYCONTROLLER_H

#include "DisplayManager.h"
#include "../api/DepartureData.h"
#include "../config/AppConfig.h"
#include <WiFi.h>

// ============================================================================
// Display State Machine Controller
// ============================================================================
/**
 * DisplayController handles the decision logic for what to display based on
 * system state. It acts as the Controller layer, keeping DisplayManager as
 * a pure View layer.
 *
 * Separates business logic (what to show) from presentation logic (how to show).
 */
class DisplayController
{
public:
    DisplayController(DisplayManager& dm);

    /**
     * Render appropriate display based on current system state
     *
     * State priority (highest to lowest):
     * 1. Demo mode (overrides everything)
     * 2. Rest mode (display off)
     * 3. WiFi connecting
     * 4. Setup required (no API key)
     * 5. API error
     * 6. No departures (loading or empty)
     * 7. Normal operation (show departures)
     *
     * @param departures Array of departures
     * @param departureCount Number of valid departures
     * @param numToDisplay How many rows to show (1-3)
     * @param wifiConnected WiFi connection status
     * @param apModeActive AP mode active (shows credentials)
     * @param apSSID AP network name
     * @param apPassword AP password
     * @param apiError API error occurred
     * @param apiErrorMsg Error message from API
     * @param stopName Current stop name
     * @param apiKeyConfigured API key/config present
     * @param demoModeActive Demo mode active
     * @param restModeActive Rest mode active (display off)
     * @param departuresLoading Awaiting first fetch (shows loading instead of no departures)
     */
    void render(const Departure* departures, int departureCount, int numToDisplay,
                bool wifiConnected, bool apModeActive,
                const char* apSSID, const char* apPassword,
                bool apiError, const char* apiErrorMsg,
                const char* stopName, bool apiKeyConfigured,
                bool demoModeActive, bool restModeActive,
                bool departuresLoading);

private:
    DisplayManager& displayManager;
};

#endif // DISPLAYCONTROLLER_H
