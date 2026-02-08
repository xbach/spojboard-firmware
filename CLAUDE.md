# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Important Guidelines

**Before performing these actions, always consult the referenced instruction files:**

- **Creating commits**: See [commit-msg.md](commit-msg.md) for commit message format and guidelines
- **Updating CLAUDE.md**: See [update-claude-md.md](update-claude-md.md) for when and how to update this file

## Project Overview

**SpojBoard** - Smart Panel for Onward Journeys

ESP32-based transit departure display that fetches real-time data from multiple transit APIs. Modular Arduino/PlatformIO project supporting multiple hardware variants with HUB75 LED matrix panels (128x32 display).

**SPOJ** = **S**mart **P**anel for **O**nward **J**ourneys (also "spoj" = connection/service in Czech)

**Supported Hardware:**
- **MatrixPortal S3** (`matrixportal_s3`) - Adafruit MatrixPortal ESP32-S3 with built-in HUB75 connector
- **ESP32-S3 N8R2** (`esp32_s3_n8r2`) - Generic ESP32-S3-DevKitC with 8MB flash, manual wiring

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
- Rest mode for scheduled or manually triggered display power saving (configurable time periods)

## Build & Development Commands

**IMPORTANT:** PlatformIO must be run from the Python virtual environment. Activate it first:
```bash
source ~/code/esp32/venv/bin/activate
```

### Building Firmware
```bash
# Build all hardware variants (output: dist/)
./build.sh

# Build specific variant only
./build.sh -e matrixportal_s3
./build.sh -e esp32_s3_n8r2

# Clean dist/ before building
./build.sh -c

# Show help
./build.sh -h
```

**Output files:**
```
dist/spojboard-matrixportal_s3-r{release}-{buildid}.bin
dist/spojboard-esp32_s3_n8r2-r{release}-{buildid}.bin
```

### Uploading to Device
```bash
# Upload to connected device (auto-detects environment from board)
pio run -e matrixportal_s3 -t upload
pio run -e esp32_s3_n8r2 -t upload

# Upload and monitor
pio run -e matrixportal_s3 -t upload && pio device monitor
```

### Debugging
```bash
# Monitor serial output (115200 baud) with exception decoder
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

### Dual-Core Architecture (ESP32-S3)

**Implementation Date:** February 2026 (Updated: February 2026 with API fetch task)

SpojBoard utilizes both cores of the ESP32-S3 for optimal performance. Core 0 handles WiFi interrupt handlers with minimal blocking, while Core 1 runs all application tasks (display rendering, API fetching, web server).

#### Core Distribution

```
┌─────────────────────────────────────────────────┐
│ CORE 0 (WiFi Network Stack)                     │
├─────────────────────────────────────────────────┤
│ • WiFi interrupt handlers (ESP-IDF default)     │
│   - Sub-millisecond response time required      │
│   - Must not be blocked by application tasks    │
│ • LwIP TCP/IP stack (ESP-IDF default)           │
│ • Network DMA/interrupt processing              │
│ • NO application tasks on this core             │
└─────────────────────────────────────────────────┘
                     ↑
                     │ WiFi/Network I/O
                     │
┌─────────────────────────────────────────────────┐
│ CORE 1 (Application Tasks)                      │
├─────────────────────────────────────────────────┤
│ • displayRenderTask() [Priority 2]              │
│   - Waits for task notification                 │
│   - Copies departure data via displayMutex      │
│   - Renders to HUB75 display via DMA (~100ms)   │
│   - Stack: 8KB                                  │
│                                                 │
│ • apiFetchTask() [Priority 1]                   │
│   - Handles blocking HTTP calls (200-2000ms)    │
│   - Updates departures[] via apiDataMutex       │
│   - Periodic fetch intervals (30s default)      │
│   - Stack: 8KB                                  │
│                                                 │
│ • Arduino loop() [Priority 1]                   │
│   - webServer.handleClient() - stays responsive │
│   - recalculateETAs() - mutex protected         │
│   - signalDisplayUpdate() helper                │
│   - State management & business logic           │
└─────────────────────────────────────────────────┘
```

#### Key Components

**Global Variables (main.cpp):**
```cpp
// Display Task Infrastructure
TaskHandle_t displayTaskHandle = NULL;           // Display task handle (Core 1)
SemaphoreHandle_t displayMutex = NULL;           // Thread-safe display access
struct DisplayUpdateRequest displayRequest;      // Snapshot of display state

// API Fetch Task Infrastructure
TaskHandle_t apiFetchTaskHandle = NULL;          // API task handle (Core 1)
SemaphoreHandle_t apiDataMutex = NULL;           // Thread-safe data access
struct APIFetchRequest apiFetchRequest;          // Fetch signals & timing
```

**DisplayUpdateRequest Structure:**
```cpp
struct DisplayUpdateRequest {
    Departure departures[MAX_DEPARTURES];  // Departure data snapshot
    int departureCount;                    // Number of departures
    int numDepartures;                     // Display limit (1-3)
    bool wifiConnected;                    // Network status
    bool apMode;                           // AP mode flag
    char apSSID[64];                       // AP credentials
    char apPassword[64];
    bool apiError;                         // Error state
    char apiErrorMsg[64];
    char stopName[64];                     // Stop name
    bool cityConfigured;                   // Config validation
    bool demoMode;                         // Demo mode flag
    bool restMode;                         // Rest mode flag
    bool needsUpdate;                      // Update pending flag
};
```

**APIFetchRequest Structure:**
```cpp
struct APIFetchRequest {
    bool fetchDeparturesNow;               // Signal immediate departures fetch
    bool fetchWeatherNow;                  // Signal immediate weather fetch
    unsigned long lastDeparturesFetch;     // Timestamp of last departures fetch
    unsigned long lastWeatherFetch;        // Timestamp of last weather fetch
};
```

**Core 1 Display Task (`displayRenderTask`):**
- Blocks on `xTaskNotifyWait()` until signaled
- Acquires displayMutex to safely copy displayRequest to local variables
- Releases mutex immediately (minimizes lock time ~1ms)
- Renders display with local copy (no mutex held during rendering ~100ms)
- Yields CPU after each update
- **Priority 2** - higher than API task and loop

**Core 1 API Fetch Task (`apiFetchTask`):**
- Sleeps 100ms between checks (non-blocking, allows web server to run)
- Checks for immediate fetch signals (`fetchDeparturesNow`, `fetchWeatherNow`)
- Checks periodic fetch intervals (30s departures, 15min weather)
- Performs blocking HTTP calls (200-2000ms depending on API)
- Acquires apiDataMutex to safely update global departures[] and weatherData
- Releases mutex immediately after update
- Signals display update via `signalDisplayUpdate()`
- **Priority 1** - same as loop, allows preemption by display task

**Main Loop Helper (`signalDisplayUpdate`):**
- Acquires displayMutex with 50ms timeout
- Copies current state (departures, config, status) to displayRequest
- Sets `needsUpdate` flag
- Releases mutex
- Notifies display task via `xTaskNotify()`

**Main Loop Helper (`recalculateETAs`):**
- Acquires apiDataMutex with 100ms timeout
- Recalculates ETAs from cached departure timestamps
- Filters and sorts departures
- Releases mutex
- Signals display update

#### Thread Safety Mechanisms

1. **Two-Mutex System:**
   - `displayMutex` - Protects displayRequest struct (display task ↔ loop communication)
   - `apiDataMutex` - Protects departures[] and weatherData (API task ↔ loop coordination)
   - Short lock durations (~1ms) - only during data copy
   - Timeout handling prevents deadlocks (50-100ms timeouts)

2. **Data Snapshot Pattern:**
   - API task copies results to global departures[] array (protected by apiDataMutex)
   - Loop copies departures[] to displayRequest struct (protected by both mutexes)
   - Display task copies displayRequest to local variables (protected by displayMutex)
   - Rendering uses local copy (no mutex held during slow operations)
   - Prevents race conditions and data corruption

3. **Task Notification:**
   - Loop signals display task via `xTaskNotify()`
   - Non-blocking from loop perspective (<1ms)
   - Display task blocks efficiently until signaled
   - Overwrites pending notifications (eSetValueWithOverwrite)

4. **Priority-Based Preemption:**
   - Display task (Pri 2) can preempt API task (Pri 1) and loop (Pri 1)
   - API task yields every 100ms to allow web server to run
   - Loop runs web server handlers with minimal blocking

#### Performance Benefits

**Before Multi-Task Architecture (Single-threaded loop):**
```
loop(): [API HTTP 1500ms]──[BLOCKED]──[Web Request]──[Display Render 100ms]──[BLOCKED]
                ↑                                              ↑
        Web server frozen                            Web server frozen again
```

**After Multi-Task Architecture (Feb 2026):**
```
Core 0: [WiFi Interrupts <1ms response]────────────────────────────────────────→
                ↑ Never blocked!

Core 1:
  API Task:     [HTTP 1500ms]──────────┐
                                       │ apiDataMutex
  Loop:         [Web Request]──────────┘──[Signal 0.5ms]──[Web Request]──→
                     ↑ Responsive!                ↓ displayMutex
  Display Task:                              [Render 100ms]──→
                                            (Preempts API task)
```

**Measured Improvements:**
- 🚀 **WiFi interrupt latency <1ms** - Core 0 never blocked by app tasks
- 🚀 **Web server always responsive** - No blocking during 1-2s API calls
- 🚀 **Display updates queued <1ms** - Non-blocking signaling
- 🚀 **Better task isolation** - Display, API, and web server run independently
- 🚀 **Priority-based scheduling** - Display (Pri 2) > Web/API (Pri 1)

#### Setup Sequence

**In `setup()` function:**
1. Initialize mutexes via `xSemaphoreCreateMutex()`:
   ```cpp
   displayMutex = xSemaphoreCreateMutex();  // Display task ↔ loop
   apiDataMutex = xSemaphoreCreateMutex();  // API task ↔ loop
   ```
2. Load configuration and initialize display
3. Create display task on Core 1 (Priority 2):
   ```cpp
   xTaskCreatePinnedToCore(
       displayRenderTask,      // Task function
       "DisplayRender",        // Name (for debugging)
       8192,                   // Stack size (8KB)
       NULL,                   // Parameters
       2,                      // Priority (higher - preempts other tasks)
       &displayTaskHandle,     // Task handle
       1                       // Core 1 (app core)
   );
   ```
4. Create API fetch task on Core 1 (Priority 1):
   ```cpp
   xTaskCreatePinnedToCore(
       apiFetchTask,           // Task function
       "APIFetch",             // Name (for debugging)
       8192,                   // Stack size (8KB)
       NULL,                   // Parameters
       1,                      // Priority (same as loop)
       &apiFetchTaskHandle,    // Task handle
       1                       // Core 1 (app core)
   );
   ```
5. Continue with WiFi setup and normal initialization
6. Signal initial API fetch via `apiFetchRequest.fetchDeparturesNow = true`

#### Usage Pattern

**Old (Single-threaded loop):**
```cpp
// Blocking API calls in loop()
if (now - lastApiCall >= interval) {
    lastApiCall = now;
    fetchDepartures();  // Blocks for 1-2 seconds, freezes web server
}

// Blocking display render in loop()
if (needsDisplayUpdate) {
    displayController.render(...);  // Blocks for 100ms
}
```

**New (Multi-task architecture):**
```cpp
// Non-blocking API fetch via task signal
apiFetchRequest.fetchDeparturesNow = true;  // Returns instantly, API task handles it

// Non-blocking display update via task signal
signalDisplayUpdate();  // Returns in <1ms, display task renders on Core 1
```

**Key Changes:**
- All `fetchDepartures()` and `fetchWeather()` calls removed from loop()
- API timing logic moved to `apiFetchTask()` on Core 1
- Loop signals immediate fetch via `apiFetchRequest` flags
- All `displayController.render()` calls replaced with `signalDisplayUpdate()`

#### Debugging & Monitoring

**Verify Core Assignment:**
```cpp
// Add to displayRenderTask() for verification:
debugPrint("DisplayTask running on Core ");
debugPrintln(xPortGetCoreID());  // Should print "0"

// Add to loop() once:
static bool printedCore = false;
if (!printedCore) {
    debugPrint("Main loop running on Core ");
    debugPrintln(xPortGetCoreID());  // Should print "1"
    printedCore = true;
}
```

**Monitor Performance:**
```cpp
// In signalDisplayUpdate() - measure copy time:
unsigned long start = micros();
// ... copy data ...
unsigned long elapsed = micros() - start;
debugPrint("Signal time: ");
debugPrint(elapsed);
debugPrintln(" μs");  // Should be <500μs
```

**Serial Output to Watch For:**
```
Display mutex created
API data mutex created
DisplayTask: Started on Core 1
Display task created on Core 1
APIFetchTask: Started on Core 1
API fetch task created on Core 1
APIFetchTask: Fetching departures (blocking)...
APIFetchTask: Departures fetch complete
DisplayTask: Rendering on Core 1
DisplayTask: Render complete
ETA Recalc: Complete, display update triggered
```

#### Troubleshooting

**Symptom:** Mutex timeout warnings in serial log
```
signalDisplayUpdate: Mutex timeout, update skipped
```
**Solution:** Increase timeout in `signalDisplayUpdate()` from 50ms to 100ms:
```cpp
if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(100)))
```

**Symptom:** Display shows corrupted/partial data
**Cause:** Race condition in data copy
**Solution:** Verify all fields are copied in `signalDisplayUpdate()` - add any new fields

**Symptom:** Device crashes/reboots during display updates
**Cause:** Stack overflow in display task
**Solution:** Increase stack size in `xTaskCreatePinnedToCore()` from 8192 to 12288:
```cpp
xTaskCreatePinnedToCore(
    displayRenderTask,
    "DisplayRender",
    12288,  // Increased from 8192
    NULL, 2, &displayTaskHandle, 0
);
```

**Symptom:** Display updates slower than expected
**Cause:** Mutex contention or priority issues
**Solution:**
1. Check for long mutex hold times (should be <1ms)
2. Verify task priority is appropriate (2 is typical)
3. Ensure display operations don't hold mutex

**Symptom:** "DisplayTask: Mutex timeout, skipping update" in logs
**Cause:** Loop holding displayMutex too long
**Solution:** Review `signalDisplayUpdate()` - ensure quick copy, no blocking operations inside mutex

**Symptom:** "ETA Recalc: Failed to acquire mutex, skipping" in logs
**Cause:** API task holding apiDataMutex too long
**Solution:** Review `apiFetchTask()` - ensure quick copy after API call, release mutex before signaling display

**Symptom:** Web server still freezes during API calls
**Cause:** API fetch task blocking too long without yielding
**Solution:** Verify `vTaskDelay(pdMS_TO_TICKS(100))` is present at end of API task loop

**Symptom:** "APIFetchTask: Failed to acquire mutex" in logs
**Cause:** ETA recalculation holding apiDataMutex too long
**Solution:** Increase timeout or reduce time spent in mutex-protected section

#### Best Practices

1. **Never hold mutex during slow operations**
   - Copy data quickly, release mutex immediately
   - Do rendering/network/HTTP outside mutex-protected sections
   - Typical mutex hold time: <1ms
   - HTTP calls take 200-2000ms - NEVER inside mutex!

2. **Keep data structures in sync**
   - When adding display parameters, update DisplayUpdateRequest struct
   - Update both `signalDisplayUpdate()` and `displayRenderTask()` copy logic
   - When adding API data fields, update both `apiFetchTask()` and `recalculateETAs()` mutex sections

3. **Monitor heap usage**
   - Both tasks use stack (8KB each), not heap
   - Check `ESP.getFreeHeap()` regularly
   - Typical free heap: ~200KB
   - Watch for stack overflow (device reboots)

4. **Debug mode logging**
   - Enable `config.debugMode` for detailed task logging
   - Use telnet for real-time log monitoring
   - Serial output shows all task activity with timestamps
   - Look for "APIFetchTask:", "DisplayTask:", "ETA Recalc:" prefixes

5. **Stack size tuning**
   - Display task: 8KB (8192 bytes)
   - API task: 8KB (8192 bytes)
   - Increase if stack overflow occurs
   - Monitor with `uxTaskGetStackHighWaterMark()`

6. **Priority management**
   - Display task (Pri 2) should remain highest application priority
   - API task and loop (Pri 1) should be equal for fair scheduling
   - Never set application tasks to priority 3+ (interferes with WiFi)

7. **Task yielding**
   - API task yields every 100ms via `vTaskDelay()`
   - Ensures web server gets CPU time
   - Never busy-wait in tasks

#### Code Locations

- **Task implementations:**
  - `src/main.cpp:403-541` (`apiFetchTask()` - API fetch task)
  - `src/main.cpp:546-640` (`displayRenderTask()` - display rendering task)
  - `src/main.cpp:650-675` (`signalDisplayUpdate()` - display update helper)
  - `src/main.cpp:110-185` (`recalculateETAs()` - ETA recalculation with mutex)
- **Infrastructure:**
  - `src/main.cpp:50-69` (display task handle, mutex, DisplayUpdateRequest struct)
  - `src/main.cpp:71-83` (API task handle, mutex, APIFetchRequest struct)
- **Task creation:**
  - `src/main.cpp:708-733` (displayMutex creation in `setup()`)
  - `src/main.cpp:763-785` (apiDataMutex creation in `setup()`)
  - `src/main.cpp:769-791` (display task creation in `setup()`)
  - `src/main.cpp:793-817` (API fetch task creation in `setup()`)
- **All signaling calls:** Search for `signalDisplayUpdate()` and `apiFetchRequest.fetch*Now` in `src/main.cpp`

#### Architectural Rationale

**Why Core 1 for All Application Tasks?**

Initial assumption was to put display rendering on Core 0 with WiFi stack. However, testing revealed:
- **WiFi interrupt handlers need <1ms response time** on Core 0
- Display rendering (100ms) and API calls (1-2s) block WiFi interrupts
- Blocked interrupts → poor network performance, web server freezes

**Solution: Keep Core 0 minimal (WiFi only), run all app tasks on Core 1:**
- Core 0: Only WiFi/LwIP interrupt handlers (ESP-IDF default)
- Core 1: Display task (Pri 2), API task (Pri 1), loop (Pri 1)
- FreeRTOS preemptive scheduler handles priority-based task switching
- Display task can preempt API task and loop when signaled
- API task yields every 100ms to allow web server to respond

**Why Separate API Fetch Task?**

Originally, API calls ran in `loop()` synchronously:
- Problem: 1-2 second blocking HTTP calls froze `webServer.handleClient()`
- Solution: Move to dedicated task that yields CPU regularly
- Web server stays responsive even during long API calls
- API timing logic isolated in single task (easier to maintain)

**Benefits of This Architecture:**
1. **WiFi stays fast** - Core 0 never blocked by app code
2. **Web server responsive** - API task yields, doesn't block loop
3. **Display updates smooth** - Separate task with higher priority
4. **Better isolation** - Each concern (WiFi, API, display, web) independent

### State Machine

The device operates in two modes with an optional demo state:
- **AP Mode** (`apModeActive=true`): Creates WiFi network for setup, DNS captive portal active, display shows credentials, demo available
- **STA Mode** (`apModeActive=false`): Connects to configured WiFi, fetches departures every 30s (configurable), serves web UI, demo available
- **Demo Mode** (`demoModeActive=true`): Pauses API polling and automatic display updates, shows custom sample departures, status visible in web UI
- **Rest Mode** (`restModeActive=true`): Display turned off, can be triggered manually or by scheduled time periods, status visible in web UI with differentiation between manual and scheduled activation

Transitions:
- Boot → Try STA mode → If fail (20 attempts/~10s) → AP mode
- AP mode + config save → Restart → Try STA mode
- STA mode connection loss → Auto-reconnect attempts every 30s
- Demo start → Set demoModeActive=true, stop API polling
- Demo stop → Set demoModeActive=false, resume normal operation
- Rest mode manual toggle → Set restModeActive=true/false, restModeManual=true
- Rest mode scheduled activation → Set restModeActive=true, restModeManual=false

### Display Rendering System

- **Row-based layout**: 4 rows × 8 pixels each on 128×32 matrix
  - Rows 0-2: Departure entries (line number, destination, ETA) - shared by normal and demo modes
  - Row 3: Date/time status bar (e.g., "08.02. Donnerstag ☀ 15° 14:23")
    - **Recent Update (Feb 2026)**: Enhanced status bar layout with full day names and numeric dates
    - Date: Fixed-width numeric format "DD.MM." (6 chars) for predictable positioning
    - Day: Full localized day names (Sunday/Sonntag/Neděle) using condensed font
    - Weather: Icon + temperature (shifted +6px right from X=65 to X=71 to accommodate longer day names)
    - Time: Original font and position (X=102) - unchanged
    - Font: Condensed font (DepartureMonoCondensed5pt8b) for date/day to fit German "Donnerstag" (10 chars)
- **Uniform route boxes**: All line numbers displayed in 18-pixel wide black background boxes (fits 1-3 characters)
  - Line numbers are preformatted (zero-padded to consistent width) before rendering
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
- JSON deserialization buffer sizes vary by API:
  - GolemioAPI (Prague): 12KB - handles busy stops with many departures
  - BvgAPI (Berlin): 24KB - BVG responses are verbose (~1.7KB per departure)
  - MqttAPI: 8KB
  - WeatherAPI: 2KB
  - GitHubOTA: 8KB
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
- JSON buffer: 12KB (handles busy stops with many departures)
- Rate limits: Configurable refresh interval (10-300s) to avoid HTTP 429
- Retry logic: 3 attempts with exponential backoff (2s, 4s, 6s), skips retry on 4xx client errors
- Find IDs at: https://data.pid.cz/stops/json/stops.json

### BVG API (Berlin)
- Endpoint: `https://v6.bvg.transport.rest/stops/{stopId}/departures`
- Authentication: None required (public API)
- Query parameters: `duration=120`, `results=12`
- Response format: JSON with departures array
- Stop ID format: Numeric stop IDs (e.g., "900013102")
- Configuration fields: `config.berlinStopIds` (no API key needed)
- JSON buffer: 24KB (BVG responses are verbose, ~1.7KB per departure)
- Retry logic: 3 attempts with exponential backoff (2s, 4s, 6s), skips retry on 4xx client errors
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
  - Cyan: < 8°C (cold)
  - White: 8-16°C (mild)
  - Yellow: 17-25°C (warm)
  - Red: > 25°C (hot)

### Multi-Stop Behavior
- Multiple stops supported via comma separation (max 12 stops)
- Each stop queried individually with separate API calls (1s delay between calls)
- All departures collected, sorted by ETA, filtered by minimum departure time, then top N displayed
- Applies to Prague and Berlin APIs (not MQTT - server handles aggregation)

### HTTP Response Handling

Located in `/src/utils/HttpUtils.{h,cpp}`:
- `readHttpResponse()` - Unified HTTP response handler for both chunked and non-chunked responses
- **Automatic chunked transfer encoding detection**: Detects when Content-Length == -1
- **Memory-efficient streaming**: Reads responses in 512-byte chunks instead of loading entire response into memory
- **Timeout protection**: 5-second max wait between chunks prevents hanging on slow/stalled connections
- **Buffer size limits**: Enforces maximum response size per API (12KB Prague, 24KB Berlin, etc.)
- **Debug logging**: Detailed chunk-by-chunk logging when debug mode enabled
- Applies to Prague (Golemio) and Berlin (BVG) APIs
- Reduces peak memory usage during API calls - critical for ESP32 stability

**Implementation Details**:
- Chunked mode: Parses hex chunk sizes, reads chunk data, handles trailing CRLF
- Non-chunked mode: Uses Content-Length header to read exact number of bytes
- Both modes handle connection timeouts and incomplete responses gracefully

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

## Hardware Variants

The firmware supports multiple hardware variants with automatic pin mapping. Each variant is built as a separate firmware binary.

### Variant Configuration (platformio.ini)

```ini
[env:matrixportal_s3]     # HARDWARE_VARIANT=1
board = adafruit_matrixportal_esp32s3
custom_hardware_variant = matrixportal_s3

[env:esp32_s3_n8r2]       # HARDWARE_VARIANT=2
board = esp32-s3-devkitc-1
custom_hardware_variant = esp32_s3_n8r2
```

### Pin Mapping

Pin definitions are in `src/config/AppConfig.h` using compile-time `#if HARDWARE_VARIANT` conditionals:

**MatrixPortal S3** (HARDWARE_VARIANT=1):
- Built-in HUB75 connector with fixed pin mapping
- R1=42, G1=40, B1=41, R2=38, G2=37, B2=39
- A=45, B=36, C=48, D=35, E=21, CLK=2, LAT=47, OE=14

**ESP32-S3 N8R2** (HARDWARE_VARIANT=2):
- Standard HUB75 wiring to GPIO pins
- Uses same physical GPIO numbers but swaps G1/B1 and G2/B2 in software
- Allows standard HUB75 rainbow cables without wire swapping

See `docs/WIRING.md` for complete wiring guide.

## Hardware Constraints

- **WiFi**: 2.4GHz only (ESP32-S3 limitation)
- **Display**: 2× HUB75 64×32 panels chained (128×32 total resolution)
- **Memory**: Custom OTA partitions (2MB app0 + 2MB app1), ~200KB typical free heap
- **Flash**: Minimum 8MB required for OTA updates
- **Clock speed**: 10MHz I2S for HUB75 communication
- **USB CDC**: Enabled on boot for serial debugging

## Web Interface Routes

- `GET /` - Main dashboard with status and config form (includes city selector)
- `POST /save` - Save configuration (triggers restart if WiFi or city changed)
- `POST /refresh` - Force immediate API call
- `POST /reboot` - Device restart
- `POST /rest-mode` - Control rest mode via REST API (JSON: {"enabled": true/false})
  - Can be triggered manually from web UI button or via external automation
  - Sets manual flag to differentiate from scheduled activation
  - Returns current state as JSON response
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
- **Status bar format** (Feb 2026 update): "08.02. Donnerstag ☀ 15° 14:23"
  - Numeric date (DD.MM.) + full localized day name + weather + time
  - Localization: `getLocalizedDayFull()` returns full day names in English, Czech, or German
  - Replaces old format: "Wed 08.Jan 14:35" (3-char day abbreviations + month abbreviations)
  - Memory optimization: Removed unused day/month abbreviations (~600 bytes flash savings)

### Critical: Timezone Initialization Timing

**⚠️ IMPORTANT:** Timezone MUST be configured before any timestamp parsing occurs!

ESP32 has dual-core architecture, and timezone initialization timing is critical:

#### The Problem
- **Core 0**: Runs `setup()` - WiFi, web server, NTP, timezone config
- **Core 1**: Runs API fetch task - can start **before** `setup()` completes
- **Race condition**: If API task parses timestamps before `configTime()` is called, it uses UTC instead of CET

#### The Fix
Call `initTimeSync()` **immediately** after WiFi connects, before any other initialization:

```cpp
// main.cpp:851-865 - CRITICAL ORDER
if (wifiConnected)
{
    // 1. Configure timezone FIRST (synchronous, immediate)
    initTimeSync();  // Calls configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER)
    apiFetchRequest.timezoneInitialized = true;  // Allow API fetches

    // 2. Then start other services (web server, OTA, etc.)
    webServer.begin();
    // ...

    // 3. Finally wait for NTP sync (asynchronous)
    syncTime(10, 500);  // Waits for actual time from NTP server
}
```

#### Why This Works
- `configTime()` sets timezone parameters for `mktime()` **synchronously** - takes effect immediately
- NTP sync happens **asynchronously** - actual time arrives later (but timezone config is already set)
- API fetches are guarded by `timezoneInitialized` flag - prevents fetches before timezone configured

#### Symptoms of Bug (if timezone not configured)
- Departures appear 1 hour in the past (UTC vs CET difference)
- Only first 2-3 departures affected if `configTime()` completes mid-parsing
- Negative `diffSec` values in logs
- Departures filtered out despite valid scheduled times

### Timestamp Parsing: `parseTimestamp()` Utility

**Location:** `src/utils/TimeUtils.{h,cpp}`

All API implementations MUST use the centralized `parseTimestamp()` function for timestamp parsing:

```cpp
time_t parseTimestamp(const char* timestamp, const char* format = "%Y-%m-%dT%H:%M:%S")
{
    struct tm tm;
    memset(&tm, 0, sizeof(tm));  // Initialize ALL fields (critical!)

    if (strptime(timestamp, format, &tm) == NULL)
        return -1;  // Parse failed

    tm.tm_isdst = -1;  // Let mktime() auto-determine DST
    return mktime(&tm);
}
```

#### Why This Matters

**Uninitialized struct tm bug:**
- Without `memset()`: `tm_isdst` contains garbage from stack
- With `memset()`: `tm_isdst = 0` (standard time)
- With `tm.tm_isdst = -1`: mktime() uses timezone rules to decide (CORRECT!)

**Critical fields:**
- `tm_isdst = -1`: "Let mktime() decide based on configured timezone"
- `tm_isdst = 0`: "Force standard time (CET)" - ignores CEST in summer
- `tm_isdst = 1`: "Force daylight time (CEST)" - ignores CET in winter
- `tm_isdst = garbage`: Undefined behavior!

**ESP32 timezone configuration:**
- `configTime(3600, 3600, "pool.ntp.org")` sets:
  - Base offset: 3600 seconds (UTC+1 = CET)
  - DST offset: +3600 seconds (UTC+2 = CEST)
- `mktime()` with `tm_isdst = -1` uses these offsets + date to determine if DST active

#### Usage in API Implementations

**GolemioAPI.cpp:**
```cpp
#include "../utils/TimeUtils.h"

time_t depTime = parseTimestamp(timestamp);
if (depTime == -1) {
    // Handle parse error
    return;
}
tempDepartures[tempCount].departureTime = depTime;
```

**BvgAPI.cpp:**
```cpp
#include "../utils/TimeUtils.h"

time_t depTime = parseTimestamp(when);
if (depTime == -1) {
    // Handle parse error
    return;
}
tempDepartures[tempCount].departureTime = depTime;
```

**Benefits:**
- Single source of truth for timestamp parsing
- Consistent initialization across all APIs
- Automatic DST handling
- Error handling built-in
- No code duplication
- Easy to test in isolation

### Debugging Time Issues

**Key logging functions:**

```cpp
// Check if NTP sync succeeded
if (!isTimeSynced()) {
    debugPrintln("⚠️ Time not synced - year < 2020");
}

// Log current device time
char timeStr[32];
getFormattedTime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S");
debugPrint("Device time: ");
debugPrintln(timeStr);

// Check if timezone initialized before parsing
if (!apiFetchRequest.timezoneInitialized) {
    debugPrintln("⚠️ Timezone not initialized - timestamps will be wrong!");
}
```

**Common issues:**
1. **Negative diffSec**: Timezone not configured, using UTC instead of CET
2. **All departures filtered**: Device clock at epoch (1970) or not synced
3. **First 2 deps wrong, rest correct**: Race condition - `configTime()` called mid-parsing
4. **Random parsing errors**: Uninitialized `struct tm` (missing `memset()`)

### NTP Sync vs Timezone Config

**Two separate operations:**

1. **Timezone Configuration** (`configTime()`)
   - Sets timezone offsets for `mktime()`
   - **Synchronous** - takes effect immediately
   - Called: Once at boot after WiFi connects
   - Purpose: Tell `mktime()` how to interpret local time

2. **NTP Time Sync** (`syncTime()`)
   - Fetches actual current time from NTP server
   - **Asynchronous** - takes 1-5 seconds
   - Called: Once at boot, can retry on failure
   - Purpose: Set device's actual clock

**Order matters:**
```
1. WiFi connects          → Network available
2. configTime()           → Timezone configured (mktime() ready)
3. API fetch can start    → Parsing timestamps works correctly
4. syncTime()             → Device clock set to actual time
5. NTP auto-updates       → Clock stays accurate (ESP32 handles this)
```

**Common mistake:** Waiting for NTP sync before allowing API fetches
- **Wrong:** Wait for `syncTime()` → API fetch → timestamps correct
- **Right:** Call `configTime()` → API fetch → timestamps correct (even if NTP not synced yet)
- **Why:** `mktime()` only needs timezone config, not accurate current time, to parse timestamps correctly

## Rest Mode

Rest mode allows scheduled display power saving by turning off the LED matrix during configurable time periods. It can be triggered either manually via the web UI or automatically based on configured time periods.

### Configuration
- Configure via `restModePeriods` config field (format: "HH:MM-HH:MM,HH:MM-HH:MM")
- Multiple periods supported, comma-separated
- Supports cross-midnight periods (e.g., "22:00-06:00" for overnight)

### Behavior
- Main loop checks `isInRestPeriod()` each cycle for scheduled activation
- When entering rest period: Display cleared, brightness set to 0
- When exiting rest period: Normal operation resumes automatically
- API polling continues during rest mode (data stays fresh)

### Manual Control
- **Web UI Button**: Toggle button in Actions section of dashboard
  - Button text changes based on state: "Enable Rest Mode" (orange) when inactive, "Disable Rest Mode" (red) when manually active
  - Reloads page after toggle to refresh status indicators
- **REST API Endpoint**: `POST /rest-mode` with JSON `{"enabled": true/false}`
  - Can be used by external automation systems (e.g., Home Assistant, cron jobs)
  - Returns current state as JSON response
- **Manual vs Scheduled**: Manual activation takes priority and is tracked separately via `restModeManual` flag
  - Manual activation: Display turns off immediately regardless of scheduled periods
  - Scheduled activation: Follows configured time periods
  - Status indicators differentiate between manual and scheduled activation

### Status Display
- **Dashboard Status Indicators**: Web UI displays current rest mode state in status section
  - "Rest Mode Active (Manual)" - shown when manually enabled via button or REST API
  - "Rest Mode Active (Scheduled)" - shown when activated by time period configuration
  - Warning badge styling (orange background) makes status clearly visible
- **Display State**: LED matrix is cleared and brightness set to 0 during rest mode

### Implementation
Located in `/src/utils/RestMode.{h,cpp}`:
- `isInRestPeriod(const char* restPeriods)` - Check if current time is within any rest period
- `parseTime(const char* timeStr, int& hours, int& minutes)` - Parse "HH:MM" format
- `isTimeBetween(...)` - Compare times with midnight-crossing support

Located in `/src/network/ConfigWebServer.{h,cpp}`:
- `POST /rest-mode` endpoint handles manual toggle requests
- `updateState()` method extended to pass `restModeActive` and `restModeManual` to web UI

Located in `/src/network/web/DashboardPage.{h,cpp}`:
- Status indicators render based on `restModeActive` and `restModeManual` flags
- Toggle button in Actions section uses AJAX to call REST API
- JavaScript in `ClientScripts.h` (SCRIPT_REST_MODE_TOGGLE) handles button click and page reload

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
