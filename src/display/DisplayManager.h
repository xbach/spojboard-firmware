#ifndef DISPLAYMANAGER_H
#define DISPLAYMANAGER_H

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <ESP32-HUB75-VirtualMatrixPanel_T.hpp>
#include "../config/AppConfig.h"
#include "../api/DepartureData.h"
#include "../api/TransitAPI.h"
#include "../api/TickerAPI.h"
#include "../api/WeatherAPI.h"
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

// Infotext alternation timing
static const int INFOTEXT_SCROLL_INTERVAL_MS = 100;  // Pixel scroll step interval for infotext
static const int INFOTEXT_MIN_DATETIME_MS = 3000;    // Minimum datetime display time (between cycles)
static const int INFOTEXT_FIRST_DATETIME_MS = 1000;  // Short datetime beat before the first scroll-in

// Destination layout calculation result (shared between drawDeparture and redrawDestination)
struct DestLayout {
    int destX;              // X position where destination text starts
    int platformReservedPx; // Pixels reserved for platform display
    char symbolChar;        // Platform symbol override character ('\0' if none)
    bool willShowPlatform;  // Whether platform will be drawn
    bool dualEta;           // Whether dual ETA mode is active for this departure
    bool showAbsoluteTime;  // Whether to show departure time instead of ETA (eta > 60min)
    const GFXfont *font;    // Selected font for destination text
    int maxChars;           // Maximum characters that fit in available space
    int spaceCalcEta;       // Right boundary for destination area (128 - etaArea)
};

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

// Display layout parameters (derived from panel configuration)
struct DisplayLayout {
    int displayWidth;       // Total display width in pixels (always 128)
    int displayHeight;      // Total display height in pixels (32 or 64)
    int rowHeight;          // Height of each row in pixels (always 8)
    int maxDepartureRows;   // Maximum departure rows: (displayHeight / rowHeight) - 1
    int statusBarY;         // Y coordinate of status bar top edge
    int statusBarBaseline;  // Y baseline for status bar text
    int panelCount;         // Number of physical panels (2 or 4)
    bool reducedColorDepth; // true when the HUB75 driver runs at 5-bit (any 128x64)
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
    bool begin(int brightness = 90, int panelRows = 1);

    /**
     * Set display brightness
     * @param brightness Brightness level (0-255)
     */
    void setBrightness(int brightness);

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
    MatrixPanel_I2S_DMA* getDisplay() { return dmaDisplay; }

    /**
     * Set weather data pointer for display rendering
     * @param data Pointer to WeatherData struct
     */
    void setWeatherData(const struct WeatherData* data) { if (data) weatherData = *data; }

    /**
     * Set infotext string for status bar alternation (from API data)
     * Ignored if manual override is active
     * @param text Concatenated infotexts (empty string = no infotexts)
     */
    void setInfoText(const char* text);

    /**
     * Set manual infotext override (from web UI / test page)
     * Blocks API updates until clearInfoText() is called
     * @param text Text to display (empty string = clear)
     */
    void setInfoTextManual(const char* text);

    /**
     * Clear infotext and manual override flag
     */
    void clearInfoText();

    const char* getInfoTextRaw() const { return infoTextRaw; }
    bool isInfoTextManual() const { return infoTextManual; }
    bool isInfoTextActive() const { return infoTextActive; }

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
     * Update infotext scroll/alternation state
     * Should be called frequently from main loop (~50ms)
     * @return true if status bar needs redraw
     */
    bool updateInfoText();

    /**
     * Reset all scroll states (call when departure data changes)
     */
    void resetScroll();

    // ========================================================================
    // Pure Rendering Methods (for DisplayController)
    // ========================================================================

    /**
     * Draw departures on display (pure rendering, no state logic)
     * @param departures Array of departures to display
     * @param departureCount Number of valid departures
     * @param numToDisplay Number of departures to show (1-3)
     */
    void drawDepartures(const Departure* departures, int departureCount, int numToDisplay);

    /**
     * Clear display and turn off (for rest mode)
     */
    void clearDisplay();

    /**
     * Draw date/time status bar (bottom row)
     */
    void drawClipped(const char* str, int x, int y, const GFXfont* font,
                     uint16_t color, int exclLeft, int exclRight);
    void drawDateTime(int exclLeft = -1, int exclRight = -1);

    /**
     * Draw status bar: infotext (if active and showing) or datetime
     */
    void drawStatusBar();

    /**
     * Draw AP mode screen with WiFi credentials
     * @param ssid AP network name
     * @param password AP password
     */
    void drawAPMode(const char* ssid, const char* password);

    /**
     * Draw candlestick chart for ticker mode
     * Renders OHLC candles in rows 0-2 with price display, plus status bar
     * @param ticker Ticker data with candles and price info
     */
    void drawTicker(const TickerData& ticker);

private:
    MatrixPanel_I2S_DMA* dmaDisplay;                                    // Raw DMA panel (always created)
    VirtualMatrixPanel_T<CHAIN_TOP_RIGHT_DOWN>* virtualDisplay;         // Virtual panel (only for multi-row)
    Adafruit_GFX* gfx;                                                  // Drawing surface (points to dmaDisplay or virtualDisplay)
    bool isDrawing;
    const Config* config;
    DisplayLayout layout;

    const GFXfont* fontSmall;
    const GFXfont* fontMedium;
    const GFXfont* fontCondensed;
    const GFXfont* fontWeather;  // Weather icon font

    // Weather data pointer
    struct WeatherData weatherData;

    // Infotext state (service alerts in status bar)
    char infoTextBuf[TransitAPI::MAX_INFOTEXT_LEN]; // Current infotext (converted to display encoding)
    char infoTextRaw[TransitAPI::MAX_INFOTEXT_LEN]; // Raw UTF-8 for change detection
    bool infoTextManual;                             // Manual override — ignore API updates
    bool infoTextActive;                              // Whether we have infotext to show
    bool showingInfoText;                             // Currently showing infotext (vs datetime)
    unsigned long infoTextPhaseStart;                 // When current phase (datetime/infotext) started
    ScrollState infoTextScroll;                       // Scroll state for infotext in status bar
    int infoTextWidthPx;                              // Text width in pixels for pixel-based scrolling

    // Scroll state for each departure row (max MAX_POSSIBLE_DISPLAY_ROWS rows)
    ScrollState scrollState[MAX_POSSIBLE_DISPLAY_ROWS];
    unsigned long lastScrollTick;

    // Current departures reference (for scroll redraws)
    const Departure* currentDepartures;
    int currentDepartureCount;
    int currentNumToDisplay;

    // Rendered departures buffer (maps row index to actual departure shown)
    // Used by scroll system - may differ from currentDepartures[row] due to dedup
    Departure renderedDeps[MAX_POSSIBLE_DISPLAY_ROWS];

    // Helpers that delegate to the correct display object
    void clearScreen();
    uint16_t color565(uint8_t r, uint8_t g, uint8_t b);

    // Internal drawing functions
    DestLayout calcDestLayout(const Departure& dep);
    void drawDeparture(int row, const Departure& dep);
    void redrawDestination(int row, const Departure& dep);
    void drawInfoText();    // Draw scrolling infotext in status bar
    void applyInfoText(const char* text); // Apply infotext immediately (internal)

    // Weather helper functions
    char mapWeatherCodeToIcon(int wmoCode);
    uint16_t getWeatherColor(int wmoCode);
    uint16_t getTemperatureColor(int temperature);
};

#endif // DISPLAYMANAGER_H
