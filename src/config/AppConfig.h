#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <Preferences.h>
#include <cstdint>

// ============================================================================
// Firmware Version
// ============================================================================
#define FIRMWARE_RELEASE "7"

// Build ID is injected by build script (8 hex characters)
// Generated from build timestamp using DJB2 hash algorithm
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
#define PANEL_HEIGHT 32
#define PANELS_NUMBER 2 // 128x32 total
#define MAX_POSSIBLE_DISPLAY_ROWS 7 // Maximum departure rows for largest supported display (128x64)

// ============================================================================
// Pin Mapping - Hardware Variant Specific
// ============================================================================
// MatrixPortal S3 has non-standard HUB75 connector (green/blue reversed)
// Generic ESP32-S3 boards use standard HUB75 pinout
//
// This allows users to use standard HUB75 cables without rewiring

#if HARDWARE_VARIANT == 1
// ────────────────────────────────────────────────────────────────────────────
// Adafruit MatrixPortal ESP32-S3 (Non-standard HUB75 connector)
// ────────────────────────────────────────────────────────────────────────────
#define R1_PIN 42
#define G1_PIN 40  // MatrixPortal: Green on pin position 2
#define B1_PIN 41  // MatrixPortal: Blue on pin position 3
#define R2_PIN 38
#define G2_PIN 37  // MatrixPortal: Green on pin position 6
#define B2_PIN 39  // MatrixPortal: Blue on pin position 7

#elif HARDWARE_VARIANT == 2
// ────────────────────────────────────────────────────────────────────────────
// Generic ESP32-S3 N8R2 DevKit (Standard HUB75 pinout)
// ────────────────────────────────────────────────────────────────────────────
#define R1_PIN 42
#define G1_PIN 41  // Standard: Green expects GPIO for blue position
#define B1_PIN 40  // Standard: Blue expects GPIO for green position
#define R2_PIN 38
#define G2_PIN 39  // Standard: Green expects GPIO for blue position
#define B2_PIN 37  // Standard: Blue expects GPIO for green position

#else
// ────────────────────────────────────────────────────────────────────────────
// Fallback: Use MatrixPortal S3 pins
// ────────────────────────────────────────────────────────────────────────────
#define R1_PIN 42
#define G1_PIN 40
#define B1_PIN 41
#define R2_PIN 38
#define G2_PIN 37
#define B2_PIN 39
#endif

// ============================================================================
// Address and Control Pins (Same for all variants)
// ============================================================================
#define A_PIN 45
#define B_PIN 36
#define C_PIN 48
#define D_PIN 35
#define E_PIN 21

#define LAT_PIN 47
#define OE_PIN 14
#define CLK_PIN 2

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
    char pragueStopIds[128];   // Prague stop IDs (e.g., "U693Z2P,U693Z1P")
    char berlinStopIds[128];   // Berlin stop IDs (e.g., "900013102")
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
    bool debugMode;         // Enable telnet logging and verbose output
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
