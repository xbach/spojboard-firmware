#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "../config/AppConfig.h"
#include "../api/DepartureData.h"
#include "DisplayColors.h"

// Font references
extern const GFXfont DepartureMono_Regular4pt8b;
extern const GFXfont DepartureMono_Regular5pt8b;
extern const GFXfont DepartureMono_Condensed5pt8b;
extern const GFXfont DepartureWeather4pt8b;  // Weather icon font

// Scroll timing constants
static const int SCROLL_INTERVAL_MS = 300;       // 500ms between scroll steps
static const int SCROLL_PAUSE_START_MS = 2000;   // 2s pause at start
static const int SCROLL_PAUSE_END_MS = 2000;     // 1s pause at end
static const int SCROLL_MAX_CYCLES = 1;          // Max scroll cycles before stopping until next refresh

// Scroll state per departure row
struct ScrollState {
    int offset;               // Current character offset (0 = show from start)
    int maxOffset;            // Maximum offset (destLen - maxChars)
    bool needsScroll;         // Does this row need scrolling?
    bool paused;              // Currently paused?
    bool atStart;             // True if paused at start, false if paused at end
    unsigned long lastUpdate; // Last scroll/pause update time
    int cycleCount;           // Number of completed scroll cycles
};

// ============================================================================
// Display Manager Class
// ============================================================================

class DisplayManager
{
public:
    DisplayManager();
    ~DisplayManager();

    /**
     * Initialize HUB75 display with pin configuration
     * @param brightness Initial brightness (0-255)
     * @return true if initialization succeeded
     */
    bool begin(int brightness = 90);

    /**
     * Set display brightness
     * @param brightness Brightness level (0-255)
     */
    void setBrightness(int brightness);

    /**
     * Update display with current state
     * @param departures Array of departures to display
     * @param departureCount Number of valid departures
     * @param numToDisplay Number of departures to show (1-3)
     * @param wifiConnected WiFi connection status
     * @param apModeActive AP mode status
     * @param apSSID AP network name (if in AP mode)
     * @param apPassword AP password (if in AP mode)
     * @param apiError API error status
     * @param apiErrorMsg API error message
     * @param stopName Current stop name
     * @param apiKeyConfigured Whether API key is configured
     * @param demoModeActive Whether demo mode is active (has highest priority, overrides ALL status screens)
     */
    void updateDisplay(const Departure* departures, int departureCount, int numToDisplay,
                      bool wifiConnected, bool apModeActive,
                      const char* apSSID, const char* apPassword,
                      bool apiError, const char* apiErrorMsg,
                      const char* stopName, bool apiKeyConfigured,
                      bool demoModeActive = false);

    /**
     * Draw status message (for temporary status during setup)
     * @param line1 First line of text
     * @param line2 Second line of text
     * @param color Text color
     */
    void drawStatus(const char* line1, const char* line2, uint16_t color);

    /**
     * Draw OTA firmware update progress
     * @param progress Bytes uploaded so far
     * @param total Total bytes to upload
     */
    void drawOTAProgress(size_t progress, size_t total);

    /**
     * Set configuration pointer for color mappings
     * @param cfg Pointer to Config struct
     */
    void setConfig(const Config* cfg) { config = cfg; }

    /**
     * Get pointer to display object (for direct access if needed)
     */
    MatrixPanel_I2S_DMA* getDisplay() { return display; }

    /**
     * Set weather data pointer for display rendering
     * @param data Pointer to WeatherData struct
     */
    void setWeatherData(const struct WeatherData* data) { weatherData = data; }

    /**
     * Draw demo mode display (repurposed from drawFontTest)
     * Shows sample departure data for customization testing
     * @param departures Array of sample departures to display
     * @param departureCount Number of departures (1-3)
     * @param stopName Stop name to display
     */
    void drawDemo(const Departure* departures, int departureCount, const char* stopName);

    /**
     * Update scroll positions for long destinations
     * Should be called frequently from main loop (~50ms)
     * @return true if any row was scrolled and needs redraw
     */
    bool updateScroll();

    /**
     * Reset all scroll states (call when departure data changes)
     */
    void resetScroll();

    /**
     * Clear display screen and flip buffer (for rest mode)
     */
    void clearScreen();

private:
    MatrixPanel_I2S_DMA* display;
    bool isDrawing;
    const Config* config;

    const GFXfont* fontSmall;
    const GFXfont* fontMedium;
    const GFXfont* fontCondensed;
    const GFXfont* fontWeather;  // Weather icon font

    // Weather data pointer
    const struct WeatherData* weatherData;

    // Scroll state for each departure row (max 3 rows)
    ScrollState scrollState[3];
    unsigned long lastScrollTick;

    // Current departures reference (for scroll redraws)
    const Departure* currentDepartures;
    int currentDepartureCount;
    int currentNumToDisplay;

    // Drawing functions
    void drawDeparture(int row, const Departure& dep);
    void drawDateTime();
    void drawAPMode(const char* ssid, const char* password);
    void redrawDestination(int row, const Departure& dep);

    // Weather helper functions
    char mapWeatherCodeToIcon(int wmoCode);
    uint16_t getWeatherColor(int wmoCode);
    uint16_t getTemperatureColor(int temperature);
};

#endif // DISPLAYMANAGER_H
