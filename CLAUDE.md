# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**SpojBoard** - Smart Panel for Onward Journeys

ESP32-based transit departure display that fetches real-time data from multiple transit APIs. Modular Arduino/PlatformIO project for Adafruit MatrixPortal ESP32-S3 with HUB75 LED matrix panels (128x32 display).

**SPOJ** = **S**mart **P**anel for **O**nward **J**ourneys (also "spoj" = connection/service in Czech)

**Key Features:**
- Multi-source support: Prague (Golemio API), Berlin (BVG API), and MQTT (Home Assistant integration)
- Standalone operation with direct API access or MQTT request/response pattern
- MQTT integration with configurable JSON field mappings and dual ETA modes (timestamp/pre-calculated)
- WiFi captive portal for configuration
- Persistent settings in ESP32 NVS flash with backward compatibility migration
- Custom 8-bit ISO-8859-2 GFXfonts with full character support (Czech, German, etc.)
- UTF-8 to ISO-8859-2 automatic conversion for API data
- Configurable minimum departure time filter with dual filtering (server-side + device-side recalculation)
- Position-based wildcard pattern matching for line colors
- Demo mode with customizable sample departures
- Web-based configuration interface with data source selector
- GitHub-based OTA firmware updates with user confirmation
- Weather display with Open-Meteo API integration (temperature and weather icon in status bar)
- Rest mode for scheduled display power saving (configurable time periods)

## Build & Development Commands

**IMPORTANT:** PlatformIO must be run from the Python virtual environment. Activate it first:
```bash
source ~/code/esp32/venv/bin/activate
```

### Building and Uploading
```bash
# Build the project
pio run

# Upload to device
pio run -t upload

# Build and upload in one command
pio run -t upload

# Clean build files
pio run -t clean

# Monitor serial output (115200 baud)
pio device monitor

# Upload and monitor
pio run -t upload && pio device monitor
```

### Debugging
```bash
# Monitor with exception decoder (configured in platformio.ini)
pio device monitor

# Check project configuration
pio project config

# List connected devices
pio device list
```

## Code Style

The project uses VSCode's C/C++ extension formatter (Microsoft style). Configuration files committed to repo:

- **`.clang-format`** - C/C++ formatting rules (Microsoft base style, 4-space indent, 120 char lines)
- **`.editorconfig`** - Cross-editor settings (UTF-8, LF line endings, indent sizes per file type)

Key style rules:
- 4-space indentation (no tabs)
- Opening brace on same line for arrays/structs, own line for functions/control statements
- Pointer/reference aligned left (`int* ptr`, `const Departure& dep`)
- 120 character line limit
- Includes not auto-sorted (preserve manual ordering)

To format: Use VSCode's "Format Document" (`Shift+Alt+F`) with C/C++ extension as default formatter.

## Architecture

### Modular Structure

The application follows a layered, modular architecture with zero circular dependencies:

```
src/
├── main.cpp                          # Application orchestration with runtime API selection
├── config/
│   ├── AppConfig.h/cpp              # Configuration structure & NVS persistence with migration
├── display/
│   ├── DisplayManager.h/cpp         # Display rendering & layout
│   ├── DisplayColors.h/cpp          # Color system & position-based wildcard matching
├── api/
│   ├── TransitAPI.h                 # Abstract base class for transit APIs
│   ├── GolemioAPI.h/cpp             # Prague Golemio API client
│   ├── BvgAPI.h/cpp                 # Berlin BVG API client
│   ├── MqttAPI.h/cpp                # MQTT API client with configurable field mappings
│   ├── WeatherAPI.h/cpp             # Open-Meteo weather API client
│   ├── DepartureData.h/cpp          # Data structures & utilities
├── network/
│   ├── WiFiManager.h/cpp            # WiFi connection & AP mode
│   ├── CaptivePortal.h/cpp          # DNS server & captive portal
│   ├── ConfigWebServer.h/cpp        # Web interface with city selector
│   ├── OTAUpdateManager.h/cpp       # OTA firmware upload handling
│   ├── GitHubOTA.h/cpp              # GitHub releases integration
└── utils/
    ├── Logger.h/cpp                 # Logging utilities
    ├── TimeUtils.h/cpp              # NTP sync & time formatting
    ├── RestMode.h/cpp               # Scheduled display power saving
    ├── gfxlatin2.h/cpp              # UTF-8 to ISO-8859-2 conversion
    └── decodeutf8.h/cpp             # UTF-8 decoder
```

**Key Design Principles:**
- **Layered Dependencies**: Lower layers never depend on higher layers
- **Single Responsibility**: Each module has one clear purpose
- **Callback Pattern**: Modules communicate upward via callbacks (e.g., ConfigWebServer → main.cpp)
- **Pure Data Structures**: Config passed as parameter, not stored in modules
- **Static Allocation**: No dynamic allocation in main loop for stability
- **Interface-Based Design**: TransitAPI abstract base class enables runtime API selection

### State Machine

The device operates in two modes with an optional demo state:
- **AP Mode** (`apModeActive=true`): Creates WiFi network for setup, DNS captive portal active, display shows credentials, demo available
- **STA Mode** (`apModeActive=false`): Connects to configured WiFi, fetches departures every 30s (configurable), serves web UI, demo available
- **Demo Mode** (`demoModeActive=true`): Pauses API polling and automatic display updates, shows custom sample departures

Transitions:
- Boot → Try STA mode → If fail (20 attempts/~10s) → AP mode
- AP mode + config save → Restart → Try STA mode
- STA mode connection loss → Auto-reconnect attempts every 30s
- Demo start → Set demoModeActive=true, stop API polling
- Demo stop → Set demoModeActive=false, resume normal operation

### Display Rendering System

- **Row-based layout**: 4 rows × 8 pixels each on 128×32 matrix
  - Rows 0-2: Departure entries (line number, destination, ETA) - shared by normal and demo modes
  - Row 3: Date/time status bar with pipe separator (e.g., "Mon| Feb 15 14:35")
- **Uniform route boxes**: All line numbers displayed in 18-pixel wide black background boxes (fits 1-3 characters)
  - Route numbers horizontally centered within boxes using `getTextBounds()` with proper x1 offset compensation
  - All destinations start at fixed X position (22 pixels) for consistent vertical alignment
- **Adaptive font rendering**: Automatically switches between regular and condensed fonts for optimal display
  - Destinations ≤16 chars (or ≤15 with AC): Regular font (DepartureMono5pt8b)
  - Destinations >16 chars: Condensed font (DepartureMonoCondensed5pt8b) with 23-char capacity
  - ETA always rendered in regular font for consistency
- **Dynamic destination truncation**: Text length adjusted based on ETA display width and font choice to prevent overlap
  - Regular font: maxChars 16, reduced by 1 if ETA ≥10 or <1
  - Condensed font: maxChars 23 (or 22 if ETA ≥10 or <1)
  - Ensures destinations never overlap with ETA regardless of font used
- **Configurable line colors**: Custom color mapping system with position-based wildcard patterns
  - User can configure colors via web interface (format: "LINE=COLOR,LINE=COLOR,...")
  - Position-based wildcards: asterisks as position placeholders (9*=2-digit, 95*=3-digit, 4**=3-digit, C***=4-digit)
  - Pattern validation: no leading asterisks, no non-trailing asterisks
  - Two-pass matching: exact matches first, then patterns
  - Falls back to hardcoded defaults if no match found
  - Available colors: RED, GREEN, BLUE, YELLOW, ORANGE, PURPLE, CYAN, WHITE
  - Stored in `Config.lineColorMap[256]` field, persisted to NVS
- **Default color coding**: Hardcoded fallback colors (Metro A=green, B=yellow, C=red, S-trains=blue, night trams=cyan, etc.)
- **Custom 8-bit ISO-8859-2 GFXfonts**:
  - `DepartureMono4pt8b` (small font) - Used for compact text, line numbers, status
  - `DepartureMono5pt8b` (medium font) - Used for destinations, larger text, ETAs
  - `DepartureMonoCondensed5pt8b` (condensed font) - Automatically used for long destinations (>16 chars)
  - Full ISO-8859-2 character set (0x20-0xDF) including Czech, German, Polish, Hungarian characters (ž, š, č, ř, ň, ť, ď, ß, ẞ, etc.)
  - Located in `/fonts` directory
- **UTF-8 Conversion**: API responses in UTF-8 are automatically converted to ISO-8859-2 encoding using in-place conversion (`utf8tocp()`)
- **Non-blocking updates**: `isDrawing` flag prevents concurrent display access
- **Optional destination scrolling**: Configurable via `config.scrollEnabled` (default: off)
  - When enabled, destinations longer than `maxChars` scroll horizontally
  - Per-row scroll state tracked in `ScrollState` struct (offset, maxOffset, needsScroll, pause state)
  - Scroll timing: 300ms per step, 2s pause at start/end, max 1 cycle before stopping
  - Only rows that need scrolling are updated - short destinations incur zero overhead
  - `updateScroll()` called from main loop every 50ms, `redrawDestination()` redraws only the destination area

### Memory Management

- Display DMA buffer allocated at startup (HUB75_I2S_CFG)
- JSON deserialization uses 8KB DynamicJsonDocument
- Typical free heap: ~200KB
- NVS flash used for configuration persistence
- No dynamic allocation in main loop

### Pin Configuration

HUB75 matrix pins are hardcoded for Adafruit MatrixPortal ESP32-S3 (lines 25-40). RGB data pins (R1/G1/B1/R2/G2/B2), address pins (A/B/C/D/E), and control pins (LAT/OE/CLK) must match hardware layout.

## API Integration Details

### TransitAPI Interface
- Abstract base class: `TransitAPI` defines common interface for all transit APIs
- `APIResult` struct: departures array, count, stop name, error status
- `APIStatusCallback`: callback function type for status updates
- Runtime API selection: main.cpp selects GolemioAPI, BvgAPI, or MqttAPI based on `config.city`

### Departure Caching and Display Logic

**CRITICAL DISTINCTION**: Three different limits control departure handling:

**MAX_TEMP_DEPARTURES (144 = MAX_DEPARTURES * 12)**:
- Temporary buffer for collecting departures from multiple stops
- Can hold up to 144 departures during collection phase (12 stops × 12 departures each)
- Used in `tempDepartures[]` array before sorting/filtering

**MAX_DEPARTURES (12)**:
- Defined in `DepartureData.h:10`
- **Final cache size**: Maximum departures stored in `APIResult.departures[]` array
- After collecting/sorting/filtering, top 12 departures are cached for reuse
- Both GolemioAPI and BvgAPI follow this limit

**config.numDepartures (1-3, user-configurable)**:
- User setting from web interface (default = 3)
- **Display limit only**: How many rows to render on LED matrix
- Hardware constraint: LED matrix has only 3 rows for departures (rows 0-2), row 3 is status bar
- Does NOT affect cache size - APIs always store up to 12 in cache
- Display layer (main.cpp) uses this to decide how many cached departures to show

**Architecture Flow**:
```
1. API queries multiple stops (up to 12 stops)
2. Collect all departures into tempDepartures[144] array (MAX_TEMP_DEPARTURES)
3. Sort all collected departures by ETA (ascending)
4. Filter by config.minDepartureTime
5. Copy top MAX_DEPARTURES (12) to result.departures[] cache
6. Display layer reads config.numDepartures and shows only that many rows
```

**Example with Multiple Stops**:
- User configures 2 stops, `numDepartures = 3`
- BvgAPI queries stop 1: gets 13 departures → adds to tempDepartures
- BvgAPI queries stop 2: gets 11 departures → adds to tempDepartures (total: 24)
- Sort all 24 by ETA
- Filter by minDepartureTime: 20 remain valid
- **Cached in result**: Top 12 departures (MAX_DEPARTURES limit)
- **Displayed on screen**: 3 departures (config.numDepartures)
- **Benefit**: Remaining 9 cached departures available for display cycling without API re-fetch

**Bug Fix History** (January 2026):
- BvgAPI previously limited cache to `config.numDepartures` (wrong - saved only 3!)
- Fixed to match GolemioAPI behavior: always cache up to MAX_DEPARTURES (12)
- This allows display to show/cycle through more departures without repeated API calls

### Golemio API (Prague)
- Endpoint: `https://api.golemio.cz/v2/pid/departureboards`
- Authentication: `x-access-token` header (get key at api.golemio.cz/api-keys)
- Query parameters: `ids` (comma-separated stop IDs), `total`, `minutesBefore`, `minutesAfter`
- Response format: JSON with stops array and departures array
- Stop ID format: GTFS IDs from PID data (e.g., "U693Z2P")
- Configuration fields: `config.pragueApiKey`, `config.pragueStopIds`
- Rate limits: Configurable refresh interval (10-300s) to avoid HTTP 429
- Find IDs at: https://data.pid.cz/stops/json/stops.json

### BVG API (Berlin)
- Endpoint: `https://v6.bvg.transport.rest/stops/{stopId}/departures`
- Authentication: None required (public API)
- Query parameters: `duration=120`, `results=12`
- Response format: JSON with departures array
- Stop ID format: Numeric stop IDs (e.g., "900013102")
- Configuration fields: `config.berlinStopIds` (no API key needed)
- JSON buffer: 24KB (BVG responses are verbose, ~1.7KB per departure)
- Find IDs at: https://v6.bvg.transport.rest/ (use /locations endpoint)

### MQTT API (Home Assistant / Custom)
- Endpoint: User-configurable MQTT broker (e.g., `homeassistant.local:1883`)
- Authentication: Optional username/password
- Request/Response pattern: SpojBoard publishes `"request"` to request topic, receives JSON on response topic
- Configuration fields: `config.mqttBroker`, `config.mqttPort`, `config.mqttUser`, `config.mqttPass`, `config.mqttRequestTopic`, `config.mqttResponseTopic`
- **Configurable JSON field mappings**: All field names customizable via web interface (line, destination, ETA, timestamp, platform, AC flag)
- **Dual ETA modes**:
  - **Timestamp mode** (recommended): Server sends unix timestamps (`dep` field), device recalculates ETAs every 10s
  - **ETA mode**: Server sends pre-calculated minutes (`eta` field), displayed as-is without recalculation
- JSON buffer: 8KB
- Timeout: 10 seconds for server response
- No stop IDs required - server aggregates and filters departures
- See `docs/MQTT.md` for complete integration guide with Home Assistant examples

### Weather API (Open-Meteo)
- Endpoint: `https://api.open-meteo.com/v1/forecast`
- Authentication: None required (free, open-source weather API)
- Query parameters: `latitude`, `longitude`, `hourly=temperature_2m,weathercode`, `forecast_hours=3`, `timezone=auto`
- Response format: JSON with hourly arrays for temperature and WMO weather codes
- Configuration fields: `config.weatherEnabled`, `config.weatherLatitude`, `config.weatherLongitude`, `config.weatherRefreshInterval`
- JSON buffer: 2KB
- Timeout: 8 seconds
- Retry logic: 2 attempts with exponential backoff
- Weather data cached and displayed in status bar alongside date/time

### Weather Data Structure
```cpp
struct WeatherData {
    int temperature;       // Temperature in Celsius (e.g., 15 = 15°C)
    int weatherCode;       // WMO weather code (0-99)
    time_t timestamp;      // Unix timestamp when data was fetched
    bool hasError;         // True if fetch encountered an error
    char errorMsg[64];     // Error message if hasError is true
};
```

### WMO Weather Code Mapping
Weather icons are rendered using the `DepartureWeather4pt8b` font with the following mappings:
- **Code 0**: Clear sky → sun icon ('a')
- **Codes 1-3**: Partly cloudy/cloudy → cloud icon ('b')
- **Codes 45-48**: Fog → fog icon ('f')
- **Codes 51-57**: Drizzle/light rain → drizzle icon ('g')
- **Codes 61-67**: Rain → rain icon ('d')
- **Codes 71-86**: Snow/sleet → snow icon ('e')
- **Codes 95+**: Thunderstorm → storm icon ('t')
- **Default**: Cloudy → cloud icon ('c')

### Weather Display Colors
- **Weather icon**: Colored by condition (yellow=sunny, white=cloudy, purple=fog, cyan=rain, blue=snow, red=storm)
- **Temperature**: Color-coded by value:
  - Blue: < 8°C (cold)
  - White: 8-16°C (mild)
  - Yellow: 17-25°C (warm)
  - Red: > 25°C (hot)

### Multi-Stop Behavior
- Multiple stops supported via comma separation (max 12 stops)
- Each stop queried individually with separate API calls (1s delay between calls)
- All departures collected, sorted by ETA, filtered by minimum departure time, then top N displayed
- Applies to Prague and Berlin APIs (not MQTT - server handles aggregation)

### Departure Data Structure
```cpp
struct Departure {
    char line[8];           // Route short name
    char destination[32];   // Trip headsign (with abbreviations applied)
    int eta;                // Calculated from predicted/scheduled timestamp
    bool hasAC;             // trip.is_air_conditioned (configurable via MQTT JSON field)
    bool isDelayed;         // From delay field
    int delayMinutes;       // Delay in minutes
    char platform[8];       // Platform/track number (stored but not currently displayed)
    time_t departureTime;   // Unix timestamp for ETA recalculation (MQTT timestamp mode)
}
```

**Destination Abbreviations**: Long words are automatically shortened to fit display:
- **Czech (Prague)**: "Nádraží" → "Nádr.", "nádraží" → "nádr.", "Sídliště" → "Sídl.", "Nemocnice" → "Nem."
- **German (Berlin)**: " Hauptbahnhof" → " Hbf", "Bahnhof" → "Bhf", "(Berlin)" → "(B)"

Abbreviations are applied in `DepartureData.cpp` before UTF-8 conversion to preserve diacritics.

## Hardware Constraints

- **Target board**: `adafruit_matrixportal_esp32s3` (PlatformIO board definition)
- **WiFi**: 2.4GHz only (ESP32-S3 limitation)
- **Display**: 2× HUB75 64×32 panels chained (128×32 total resolution)
- **Memory**: Default partition scheme, ~200KB typical free heap
- **Clock speed**: 10MHz I2S for HUB75 communication
- **USB CDC**: Enabled on boot for serial debugging

## Web Interface Routes

- `GET /` - Main dashboard with status and config form (includes city selector)
- `POST /save` - Save configuration (triggers restart if WiFi or city changed)
- `POST /refresh` - Force immediate API call
- `POST /reboot` - Device restart
- `GET /demo` - Demo configuration page with editable sample departures
- `POST /start-demo` - Start demo mode with custom departure data (JSON)
- `POST /stop-demo` - Stop demo mode and resume normal operation
- `GET /update` - OTA firmware upload form (manual upload)
- `POST /update` - Handle firmware file upload (split into two handlers):
  - Upload chunk handler: `handleUpdateProgress()` - processes chunks without HTTP response
  - Completion handler: `handleUpdateComplete()` - sends final HTTP response after upload finishes
- `GET /check-update` - Check GitHub for new releases (AJAX)
- `POST /download-update` - Download and install from GitHub (AJAX)
- Captive portal detection: `/generate_204`, `/hotspot-detect.html`, `/ncsi.txt`, `/success.txt`
- `404 handler` - Redirects to root (captive portal behavior)

## Configuration Storage

NVS namespace: "transport"

**General Settings:**
- `wifiSsid` (String)
- `wifiPass` (String)
- `city` (String, 16 chars) - Data source: "Prague", "Berlin", or "MQTT"
- `language` (String, 3 chars) - Language code (default: "en", future use for localization)
- `refresh` (Int, seconds, 10-300)
- `numDeps` (Int, 1-6)
- `minDepTime` (Int, minutes, 0-30) - Filter out departures below this time during ETA recalculation
- `brightness` (Int, 0-255) - Display brightness level
- `lineColorMap` (String, max 256 chars) - Custom line color mappings (format: "LINE=COLOR,LINE=COLOR,...")
  - Position-based wildcards: asterisks as position placeholders (e.g., "9*=CYAN", "4**=BLUE")
  - Falls back to hardcoded defaults if empty or no match
- `debugMode` (Bool) - Enable telnet logging
- `showPlatform` (Bool) - Display platform numbers (stored but not currently implemented)
- `scrollEnabled` (Bool) - Enable scrolling for long destination names (default: off)
- `restModePeriods` (String, 256 chars) - Rest mode time periods (format: "HH:MM-HH:MM,HH:MM-HH:MM")
- `configured` (Bool)

**Weather Settings:**
- `weatherEnabled` (Bool) - Enable weather display in status bar
- `weatherLatitude` (Float) - GPS latitude for weather location (e.g., 50.0755 for Prague)
- `weatherLongitude` (Float) - GPS longitude for weather location (e.g., 14.4378 for Prague)
- `weatherRefreshInterval` (Int, minutes) - Interval between weather API calls (default: 15)

**Prague API Settings:**
- `pragueApiKey` (String, 300 chars) - Golemio API key
- `pragueStopIds` (String, 128 chars) - Stop IDs (comma-separated)

**Berlin API Settings:**
- `berlinStopIds` (String, 128 chars) - Stop IDs (comma-separated)

**MQTT Settings:**
- `mqttBroker` (String, 128 chars) - MQTT broker address (IP or hostname)
- `mqttPort` (Int) - MQTT broker port (default: 1883)
- `mqttUser` (String, 64 chars) - Optional username for broker authentication
- `mqttPass` (String, 64 chars) - Optional password for broker authentication
- `mqttRequestTopic` (String, 128 chars) - Topic where SpojBoard publishes requests
- `mqttResponseTopic` (String, 128 chars) - Topic where SpojBoard subscribes for responses
- `mqttUseTimestamps` (Bool) - ETA mode: false = pre-calculated minutes, true = unix timestamps
- `mqttFieldLine` (String, 32 chars) - JSON field name for line number (default: "line")
- `mqttFieldDest` (String, 32 chars) - JSON field name for destination (default: "dest")
- `mqttFieldEta` (String, 32 chars) - JSON field name for ETA minutes (default: "eta")
- `mqttFieldDep` (String, 32 chars) - JSON field name for departure timestamp (default: "dep")
- `mqttFieldPlatform` (String, 32 chars) - JSON field name for platform (default: "plt")
- `mqttFieldAc` (String, 32 chars) - JSON field name for AC flag (default: "ac")

**Backward Compatibility Migration**:
- Old `apiKey` field → `pragueApiKey`
- Old `stopIds` field → `pragueStopIds`
- Automatic migration on first load with new firmware
- Old keys removed from NVS after migration

**Defaults:**
- WiFi: From DEFAULT_WIFI_SSID/PASSWORD defines
- Data source: "Prague"
- Language: "en"
- Refresh interval: 60s
- Number of departures: 3
- Minimum departure time: 3 minutes
- Brightness: 90
- Line color map: Empty (uses hardcoded defaults)
- Debug mode: Disabled
- Show platform: Disabled
- Scroll enabled: Disabled
- Rest mode periods: Empty (disabled)
- Weather enabled: Disabled
- Weather refresh interval: 15 minutes
- Weather coordinates: 0.0, 0.0 (must be configured)
- MQTT use timestamps: True (timestamp mode)
- MQTT field mappings: "line", "dest", "eta", "dep", "plt", "ac"

**First-time setup (AP Mode)**: Only WiFi credentials required, data source and configuration optional. Demo mode available before API configuration.

## Time Handling

- NTP sync: `pool.ntp.org`
- Timezone: CET/CEST (UTC+1/+2)
- ETA calculation: Compares ISO timestamp from API with local time (mktime/difftime)
- Display format: "Wed 08.Jan 14:35" on bottom row

## Rest Mode

Rest mode allows scheduled display power saving by turning off the LED matrix during configurable time periods.

### Configuration
- Configure via `restModePeriods` config field (format: "HH:MM-HH:MM,HH:MM-HH:MM")
- Multiple periods supported, comma-separated
- Supports cross-midnight periods (e.g., "22:00-06:00" for overnight)

### Behavior
- Main loop checks `isInRestPeriod()` each cycle
- When entering rest period: Display cleared, brightness set to 0
- When exiting rest period: Normal operation resumes automatically
- API polling continues during rest mode (data stays fresh)

### Implementation
Located in `/src/utils/RestMode.{h,cpp}`:
- `isInRestPeriod(const char* restPeriods)` - Check if current time is within any rest period
- `parseTime(const char* timeStr, int& hours, int& minutes)` - Parse "HH:MM" format
- `isTimeBetween(...)` - Compare times with midnight-crossing support

### Example Configurations
- `"23:00-07:00"` - Off overnight (11 PM to 7 AM)
- `"00:00-06:00,22:00-23:59"` - Off late night and early morning
- `"09:00-17:00"` - Off during work hours
- Empty string - Rest mode disabled (default)

## Font System

### Custom 8-bit ISO-8859-2 GFXfonts

Located in `/src/fonts` directory:
- **8-bit fonts (ISO-8859-2 encoding)**:
  - `DepartureMono4pt8b.h` - Small font (4pt)
  - `DepartureMono5pt8b.h` - Medium font (5pt) - default for destinations and ETAs
  - `DepartureMonoCondensed5pt8b.h` - Condensed font (5pt) - automatically used for destinations >16 chars
  - Character range: 0x20-0xDF (192 printable characters)
  - Full ISO-8859-2 support for Czech, Slovak, Polish, Hungarian, etc.
- **Weather icon font**:
  - `DepartureWeather4pt8b.h` - Weather icon font (4pt)
  - Characters 'a'-'t' map to weather icons (sun, clouds, rain, snow, fog, storm, etc.)
  - Used in status bar to display current weather condition
  - WMO weather codes mapped to icon characters via `mapWeatherCodeToIcon()`

**UTF-8 Conversion System:**
Located in `/src/utils` directory:
- `decodeutf8.cpp/h` - UTF-8 decoder (based on RFC 3629)
- `gfxlatin2.cpp/h` - Converts UTF-8 to ISO-8859-2 with GFX encoding (characters 0xA0-0xFF shifted to 0x80-0xDF)

**Usage in Code:**
```cpp
#include "../fonts/DepartureMono5pt8b.h"
#include "../utils/gfxlatin2.h"

const GFXfont* fontMedium = &DepartureMono5pt8b;

// Get UTF-8 string from API
char destination[32];
strlcpy(destination, "Nádraží Hostivař", sizeof(destination));

// Convert to ISO-8859-2 (in-place)
utf8tocp(destination);

// Display with proper Czech characters
display->setFont(fontMedium);
display->setTextColor(COLOR_WHITE);
display->setCursor(x, y);
display->print(destination);  // Correctly shows "ř" and other diacritics
```

**Font API (Adafruit GFX):**
- `display->setFont(const GFXfont*)` - Switch font
- `display->setTextColor(uint16_t)` - Set foreground color (transparent background)
- `display->setCursor(int16_t x, int16_t y)` - Position cursor
- `display->getTextBounds(const char*, int16_t, int16_t, int16_t*, int16_t*, uint16_t*, uint16_t*)` - Measure text dimensions
- `display->print(const char*)` - Render text

All fonts are stored in PROGMEM to save RAM.

**Font Generation:**
8-bit fonts are generated using the [fontconvert8-iso8859-2](https://github.com/petrbrouzda/fontconvert8-iso8859-2) tool with ISO-8859-2 encoding, which shifts extended characters (0xA0-0xFF) by -32 to fit in the 0x80-0xDF range, allowing full 8-bit character coverage.

## OTA Update System

### Overview

SpojBoard includes two methods for firmware updates:
1. **Manual Upload**: Upload .bin file via web interface (existing)
2. **GitHub Updates**: Check for and download new releases from GitHub (new)

Both methods use the ESP32's built-in OTA partition system and are **disabled in AP mode** for security.

### GitHub OTA Update System

Located in `/src/network/GitHubOTA.{h,cpp}` - standalone class for GitHub releases integration.

**Architecture:**
```
User clicks "Check for Updates"
    → GitHubOTA::checkForUpdate()
    → GitHub API: /repos/xbach/spojboard-firmware/releases/latest
    → Parse JSON, compare versions
    → Return ReleaseInfo struct
    → Display update card in UI
User clicks "Download & Install"
    → GitHubOTA::downloadAndInstall()
    → Stream firmware from GitHub to OTA partition
    → Progress displayed on LED matrix
    → MD5 validation via Update.end(true)
    → Auto-reboot on success
```

**Key Files:**
- `/src/network/GitHubOTA.h` - Class definition with ReleaseInfo struct
- `/src/network/GitHubOTA.cpp` - Implementation with streaming download
- `/src/config/AppConfig.h` - GitHub repository constants

### GitHubOTA Class

**ReleaseInfo Struct:**
```cpp
struct ReleaseInfo {
    bool available;           // Update available?
    bool hasError;           // API error occurred?
    char errorMsg[128];      // Error message
    int releaseNumber;       // Parsed release number (1, 2, 3...)
    char tagName[32];        // GitHub tag (e.g., "r1", "r2")
    char releaseName[64];    // Human-readable name
    char releaseNotes[512];  // Truncated release body
    char assetUrl[256];      // .bin download URL
    char assetName[64];      // Filename
    size_t assetSize;        // File size in bytes
};
```

**Public Methods:**
- `ReleaseInfo checkForUpdate(const char* currentRelease)` - Query GitHub API
- `bool downloadAndInstall(const char* assetUrl, size_t expectedSize, ProgressCallback onProgress)` - Stream and flash firmware

**Private Helpers:**
- `int parseReleaseNumber(const char* tagName)` - Extract number from "r1" → 1
- `bool findBinaryAsset(JsonDocument& doc, ...)` - Find .bin file in release assets
- `bool validateFirmwareFilename(const char* filename)` - Validate pattern

### Version Comparison

**Current Version:** `FIRMWARE_RELEASE` from AppConfig.h (e.g., "1")
**GitHub Tag:** Extract from `tag_name` field (e.g., "r2" → 2)
**Logic:** Compare as integers - if GitHub version > current version, update available

**Example:**
- Current: "1"
- GitHub tag: "r2" → parsed as 2
- Result: Update available

### GitHub API Integration

**Endpoint:** `https://api.github.com/repos/xbach/spojboard-firmware/releases/latest`
**Authentication:** None (60 requests/hour unauthenticated - sufficient for manual checks)
**Timeout:** 30 seconds (HTTP_TIMEOUT_MS)
**JSON Buffer:** 8KB DynamicJsonDocument

**Response Structure:**
```json
{
  "tag_name": "r2",
  "name": "Release 2",
  "body": "## Release notes...",
  "assets": [
    {
      "name": "spojboard-r2-a1b2c3d4.bin",
      "size": 1234567,
      "browser_download_url": "https://github.com/.../download/..."
    }
  ]
}
```

### Streaming Download

**Critical Design:** Firmware (~1-2 MB) is streamed directly to OTA partition without buffering entire file in RAM.

**Flow:**
1. `HTTPClient::GET(assetUrl)` - Start download
2. `http.getStreamPtr()` - Get WiFiClient stream
3. `Update.begin(contentLength)` - Initialize OTA
4. Loop: `stream->readBytes(buffer, 1024)` → `Update.write(buffer, size)`
5. `Update.end(true)` - Finalize with MD5 validation
6. `ESP.restart()` - Reboot into new firmware

**Memory Usage:**
- Download buffer: 1KB chunks
- JSON buffer: 8KB
- GitHubOTA class: ~1KB overhead
- **Total impact: ~10KB** (acceptable with ~200KB free heap)

**Progress Updates:**
- Callback every 10KB or 1% of download
- Forwards to `DisplayManager::drawOTAProgress()`
- LED matrix shows progress bar

### Security & Validation

1. **HTTPS Only:** All communication over TLS (ESP32 built-in CA store)
2. **Filename Validation:** Regex match `spojboard-r\d+-[0-9a-f]{8}\.bin`
3. **Size Validation:** Compare Content-Length with GitHub API reported size
4. **MD5 Validation:** Automatic via `Update.end(true)` - firmware rejected if invalid
5. **AP Mode Block:** Updates disabled in AP mode (security measure)
6. **Sanity Check:** Reject if GitHub release < current release

### Error Handling

**HTTP Errors:**
- 404: No releases found
- 403: GitHub API access denied
- 429: Rate limit exceeded (60/hour)
- Timeout: Network too slow

**Download Errors:**
- Size mismatch: Content-Length ≠ expected size
- Incomplete download: Connection dropped mid-transfer
- Flash error: OTA partition write failure
- MD5 mismatch: Corrupted download

**Recovery:**
- Failed update doesn't affect running firmware (separate OTA partition)
- Device remains bootable even if update fails mid-flash
- User can retry download

### Web UI Integration

**Dashboard Button:**
```html
<button id="checkUpdateBtn">Check for Updates</button>
<div id="updateStatus"></div>
```

**JavaScript Flow:**
1. Click button → `fetch('/check-update')`
2. Parse JSON response
3. If available: Show card with version, notes, size, "Download & Install" button
4. If up-to-date: Show "You're up to date!" message
5. If error: Show error message
6. Click install → `fetch('/download-update', {method: 'POST', body: JSON})`
7. Show progress UI
8. On success: "Update installed! Rebooting..."
9. Auto-reload page after 8 seconds

**User Experience:**
- No page refresh during checking (AJAX)
- Real-time progress display
- Confirmation required before download
- Clear error messages

### Release Creation Workflow

**GitHub Actions:** `.github/workflows/release.yml`

1. **Trigger:** Push tag matching `r*` (e.g., `r1`, `r2`)
2. **Build:** PlatformIO builds firmware with timestamp-based build ID
3. **Artifact:** `dist/spojboard-r{release}-{buildid}.bin`
4. **Release:** Creates GitHub release with firmware as asset
5. **Auto-publish:** Release published automatically

**Local Build:**
```bash
./build.sh
# Output: dist/spojboard-r1-37a954fd.bin
```

### Configuration

**Hardcoded Repository:** `xbach/spojboard-firmware` (not user-configurable)

**Constants in AppConfig.h:**
```cpp
#define GITHUB_REPO_OWNER "xbach"
#define GITHUB_REPO_NAME "spojboard-firmware"
```

### Testing Updates

1. Build and deploy firmware with release "1"
2. Create GitHub release with tag "r2"
3. Upload firmware .bin as asset
4. Open device dashboard at `http://[device-ip]/`
5. Click "Check for Updates"
6. Should show update available
7. Click "Download & Install"
8. Watch progress on LED matrix
9. Device reboots with new firmware
