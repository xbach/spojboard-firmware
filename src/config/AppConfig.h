#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <Preferences.h>
#include <cstdint>
#include "../api/DepartureData.h" // STOP_IDS_BUF_SIZE (stop-ID list buffer)
#include "HardwareProfile.h"     // HubPins / RgbOrder / HwProfile (TA-0302)

// ============================================================================
// Firmware Version
// ============================================================================
#define FIRMWARE_RELEASE "9"

// Build ID is injected by build script (8 hex characters)
// Generated from build timestamp using DJB2 hash algorithm
#ifndef BUILD_DIRTY
#define BUILD_DIRTY 0 // Fallback if not set by build system (scripts/build_id.py)
#endif

#ifndef BUILD_ID
#define BUILD_ID 0x00000000 // Fallback if not set by build system
#endif

// ============================================================================
// Hardware Variant Identification
// ============================================================================
// These are set by platformio.ini build_flags per environment
#ifndef HARDWARE_VARIANT
#define HARDWARE_VARIANT 1                         // Default to MatrixPortal S3
#define HARDWARE_NAME "matrixportal_s3"            // Filesystem-safe name
#define HARDWARE_DISPLAY_NAME "MatrixPortal-S3"    // Human-readable name
#endif

// Export variant info
#define FIRMWARE_VARIANT HARDWARE_VARIANT
#define VARIANT_NAME HARDWARE_NAME

#define VARIANT_DISPLAY_NAME HARDWARE_DISPLAY_NAME

// ============================================================================
// Display Variant Identification  (TA-0269 §3)
// ============================================================================
// Panel GEOMETRY is a build-time property, not a runtime setting. Panel height
// changes the address-line count and, on the MatrixPortal, the G/B pin order --
// and a runtime selector cannot express a pin map. Guessing wrong gives a dead
// display, with no working display left to diagnose it on.
//
//   1 = 2x32   2x 64x32 chained horizontally     128x32
//   2 = 4x32   4x 64x32 in a 2x2 serpentine      128x64
//   3 = 2x64   2x 64x64 chained horizontally     128x64
//
// The token is <panel count>x<panel height>, so it describes the hardware
// rather than the pixel size: 4x32 and 2x64 are BOTH 128x64 and cannot be told
// apart by resolution alone.
//
// NO DISPLAY TOKEN MAY EVER BEGIN WITH 'r'. r8's OTA parser takes the asset
// name up to the FIRST "-r", so a token like "rgb" truncates the board field
// and makes an r8 device accept another board's firmware. r9+ finds the release
// field by exact r<digits> shape, but r8 devices are in the field and cannot be
// changed, so the constraint is permanent. Pinned by test/test_r8compat.
#ifndef DISPLAY_VARIANT
#define DISPLAY_VARIANT 1 // Default: 2x32, the geometry every release has shipped
#endif

#if DISPLAY_VARIANT == 3
#define DISPLAY_VARIANT_NAME "2x64"
#define PANEL_HEIGHT 64
#define PANELS_NUMBER 2
#define DISPLAY_PANEL_ROWS 2 // 128x64
#elif DISPLAY_VARIANT == 2
#define DISPLAY_VARIANT_NAME "4x32"
#define PANEL_HEIGHT 32
#define PANELS_NUMBER 4
#define DISPLAY_PANEL_ROWS 2 // 128x64
#else
#define DISPLAY_VARIANT_NAME "2x32"
#define PANEL_HEIGHT 32
#define PANELS_NUMBER 2
#define DISPLAY_PANEL_ROWS 1 // 128x32
#endif

// ============================================================================
// GitHub OTA Configuration
// ============================================================================
#define GITHUB_REPO_OWNER "xbach"
#define GITHUB_REPO_NAME "spojboard-firmware"

// ============================================================================
// Default Line Color Map (Prague transit)
// ============================================================================
#define DEFAULT_LINE_COLOR_MAP "A=GREEN,B=YELLOW,C=RED,*=WHITE,1*=WHITE,2*=WHITE,5*=PURPLE,1**=PURPLE,2**=PURPLE,S*?=BLUE,9*?=CYAN,*???=YELLOW"

// ============================================================================
// Hardware Configuration (HUB75 Display)
// ============================================================================
#define PANEL_WIDTH 64
// PANEL_HEIGHT / PANELS_NUMBER / DISPLAY_PANEL_ROWS come from DISPLAY_VARIANT above.
#define MAX_POSSIBLE_DISPLAY_ROWS 7 // Maximum departure rows for largest supported display (128x64)

// ============================================================================
// Pin Mapping (TA-0302)
// ============================================================================
// These are the GPIOs wired to HUB75 connector POSITIONS 1..6, and they are the
// same on every supported board -- the three per-variant blocks this replaced
// were byte-identical apart from the green/blue pair. That difference was never
// about pins: it is a CHANNEL ORDER, so it is expressed as one below and is now
// a runtime setting (see HardwareProfile.h). The macros here are only the
// factory default a device falls back to.
#define R1_PIN 42
#define G1_PIN 41
#define B1_PIN 40
#define R2_PIN 38
#define G2_PIN 39
#define B2_PIN 37

// ============================================================================
// Address and Control Pins (same on every variant)
// ============================================================================
#define A_PIN 45
#define B_PIN 36
#define C_PIN 48
#define D_PIN 35
// E_PIN is the 5th address line. 32-high panels use 4 address bits and drive it
// LOW permanently; 64-high panels (2x64) NEED it. Confirmed present on the
// MatrixPortal's HUB75 connector on hardware, 2026-08-26.
#define E_PIN 21

#define LAT_PIN 47
#define OE_PIN 14
#define CLK_PIN 2

// ============================================================================
// Default RGB channel order
// ============================================================================
// The MatrixPortal's HUB75 connector presents green and blue transposed
// relative to a standard cable -- but only for 32-HIGH panels. BeerBoard
// 30c2451 found that keeping the transposition on 64x64 panels renders green
// and blue swapped (orange->pink, sky->teal), so 64-high panels want the
// standard order on the same board.
//
// Expressed as an order rather than a second pin table, this stops being a
// special case: it is the default value of a setting the user can change, which
// is what discussion #8 asked for.
#if HARDWARE_VARIANT == 1 && DISPLAY_VARIANT != 3
#define DEFAULT_RGB_ORDER RgbOrder::RBG
#elif HARDWARE_VARIANT == 2
#define DEFAULT_RGB_ORDER RgbOrder::RGB
#elif HARDWARE_VARIANT == 1
#define DEFAULT_RGB_ORDER RgbOrder::RGB
#else
#define DEFAULT_RGB_ORDER RgbOrder::RBG
#endif

// The factory pin map, in connector order. A device with no stored profile --
// or with one that fails validation -- falls back to exactly this.
inline HubPins hwCompiledDefaultPins()
{
    HubPins p;
    p.r1 = R1_PIN; p.g1 = G1_PIN; p.b1 = B1_PIN;
    p.r2 = R2_PIN; p.g2 = G2_PIN; p.b2 = B2_PIN;
    p.a = A_PIN; p.b = B_PIN; p.c = C_PIN; p.d = D_PIN; p.e = E_PIN;
    p.lat = LAT_PIN; p.oe = OE_PIN; p.clk = CLK_PIN;
    return p;
}

// Default WiFi credentials (for initial setup)
#define DEFAULT_WIFI_SSID "Your WiFi SSID"
#define DEFAULT_WIFI_PASSWORD "Your WiFi Password"

// ============================================================================
// Configuration Structure
// ============================================================================

struct Config
{
    char wifiSsid[64];
    char wifiPassword[64];

    // Per-city configuration fields
    char pragueApiKey[512];    // Golemio API key (JWT) for Prague. Older keys embed an email in
                               // the payload (~303 chars); newer keys ~224. Sized large so the
                               // variable-length email never truncates the token (→ 401). NVS
                               // stores the actual length, so the headroom is free. (Issue #5)
    char pragueStopIds[STOP_IDS_BUF_SIZE]; // Prague stop IDs (e.g., "U693Z2P,U693Z1P"); sized for 12 stops
    char berlinStopIds[STOP_IDS_BUF_SIZE]; // Berlin stop IDs (e.g., "900013102"); see STOP_IDS_BUF_SIZE
    // Note: Berlin BVG API requires no authentication

    // MQTT-specific configuration
    char mqttBroker[128];          // MQTT broker IP/hostname
    int mqttPort;                   // MQTT broker port (default 1883)
    char mqttUsername[64];         // MQTT username (optional, empty = no auth)
    char mqttPassword[64];         // MQTT password (optional)
    char mqttRequestTopic[64];     // MQTT request topic
    char mqttResponseTopic[64];    // MQTT response topic
    bool mqttUseEtaMode;           // true = ETA mode, false = Timestamp mode

    // MQTT JSON field mappings
    char mqttFieldLine[32];        // Line number field name (e.g., "line")
    char mqttFieldDestination[32]; // Destination field name (e.g., "dest")
    char mqttFieldEta[32];         // ETA field name (e.g., "eta")
    char mqttFieldTimestamp[32];   // Timestamp field name (e.g., "dep")
    char mqttFieldPlatform[32];    // Platform field name (e.g., "plt")
    char mqttFieldAC[32];          // AC flag field name (e.g., "ac")

    int refreshInterval;    // Seconds between API calls
    int numDepartures;      // Number of departures to display (1-3 rows on LED matrix)
    int minDepartureTime;   // Minimum departure time in minutes (filter out departures < this)
    int brightness;         // Display brightness (0-255)
    int panelRows;          // Panel row count: 1 = 128x32 (2 panels), 2 = 128x64 (4 panels)
    char lineColorMap[256]; // Line color mappings (format: "A=GREEN,B=YELLOW,9*=CYAN")
    char platformSymbolMap[256]; // Platform-to-arrow mappings (format: "B=3,ID:U693Z2P=7")
    char city[16];          // Transit city: "Prague" or "Berlin"
    char language[8];       // Display language: "en", "cs", "de"
    bool debugMode;         // Enable verbose HTTP/API logging on Serial
    bool showPlatform;      // Display platform/track between destination and ETA
    bool scrollEnabled;     // Enable scrolling for long destination names (default: off)
    bool showMultipleTimes; // Show next two departure times per line (default: off)
    char restModePeriods[256]; // Rest mode time periods (format: "HH:MM-HH:MM,HH:MM-HH:MM")

    // Weather configuration
    bool weatherEnabled;        // Enable weather display
    float weatherLatitude;      // GPS latitude (e.g., 50.0755 for Prague)
    float weatherLongitude;     // GPS longitude (e.g., 14.4378 for Prague)
    int weatherRefreshInterval; // Minutes between weather fetches (default: 15)

    // Ticker mode configuration (easter egg — candlestick chart display)
    bool tickerEnabled;            // Persistent enable — auto-activates on boot
    char tickerSymbol[16];         // Twelve Data symbol (e.g., "BTC/USD", "AAPL")
    char tickerInterval[8];        // Candle interval: "1h", "4h", "1day"
    char tickerApiKey[64];         // Twelve Data API key
    int tickerRefreshInterval;     // Seconds between fetches (120-600)

    // Display hardware profile (TA-0302). The wiring is a runtime setting; the
    // macros above are only the factory default. `hwProfile.pins` is always in
    // connector order -- the channel order is applied on top of it, so a
    // stock-wired panel with transposed channels needs no pin edits at all.
    HwProfile hwProfile;

    bool configured;
};

// ============================================================================
// Configuration Management Functions
// ============================================================================

/**
 * Load configuration from NVS flash storage
 * @param config Reference to Config structure to populate
 */
void loadConfig(Config &config);

/**
 * Save configuration to NVS flash storage
 * @param config Configuration to save
 */
void saveConfig(const Config &config);

/**
 * Clear all configuration from NVS flash storage
 * Resets device to factory defaults - will boot into AP mode on next restart
 */
void clearConfig();

/**
 * Verify that firmware matches hardware variant
 * On first boot, stores hardware variant to NVS
 * On subsequent boots, checks if stored variant matches compiled variant
 * @return true if hardware matches, false if mismatch detected
 */
bool verifyHardware();

#endif // APPCONFIG_H
