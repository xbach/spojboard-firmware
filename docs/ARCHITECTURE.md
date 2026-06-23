# SpojBoard Architecture & Data Flow

Technical documentation for developers and contributors.

## Table of Contents

- [Dual-Core Architecture](#dual-core-architecture)
- [Configuration Constants](#configuration-constants)
- [Complete Pipeline Flow](#complete-pipeline-flow)
- [Design Rationale](#design-rationale)
- [Memory Allocation](#memory-allocation)
- [State Machine](#state-machine)
- [Module Dependencies](#module-dependencies)

## Dual-Core Architecture

SpojBoard utilizes both cores of the ESP32-S3 for optimal performance using FreeRTOS tasks.

### Core Distribution

```
┌─────────────────────────────────────────────────┐
│ CORE 0 (WiFi Network Stack)                     │
├─────────────────────────────────────────────────┤
│ WiFi interrupt handlers (sub-ms response)       │
│ LwIP TCP/IP stack                               │
│ NO application tasks                            │
└─────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────┐
│ CORE 1 (Application Tasks)                      │
├─────────────────────────────────────────────────┤
│ displayRenderTask()  [Priority 2]               │
│   Waits for notification, copies data via       │
│   mutex, renders to HUB75 (~100ms)              │
│                                                 │
│ apiFetchTask()       [Priority 1]               │
│   Handles blocking HTTP calls (200-2000ms)      │
│   Updates departures via mutex, sleeps 100ms    │
│                                                 │
│ Arduino loop()       [Priority 1]               │
│   Web server, ETA recalculation, state mgmt    │
└─────────────────────────────────────────────────┘
```

### Thread Safety

Two mutexes protect shared data with short lock durations (~1ms):

- **`displayMutex`** - Protects `DisplayUpdateRequest` struct (display task <-> loop)
- **`apiDataMutex`** - Protects `departures[]` array and weather data (API task <-> loop)

**Data snapshot pattern**: Data is copied under mutex, then processed without locks. Rendering and HTTP calls never hold a mutex.

### Why This Design?

**Before (single-threaded):** API calls (1-2s) blocked the web server and display updates.

**After (multi-task):**
- Web server stays responsive during API calls
- Display updates queued in <1ms via `signalDisplayUpdate()`
- WiFi interrupts on Core 0 never blocked by application code
- Display task (Priority 2) preempts API task (Priority 1) when needed

### DisplayController

The `DisplayController` class acts as a state machine that determines **what** to display, delegating the **how** to `DisplayManager`:

```
DisplayController (decides what to show)
    ↓ calls appropriate method
DisplayManager (renders to LED matrix)
    ↓ uses
HUB75 Hardware
```

**Priority-based state evaluation:**
1. Demo mode (highest) - custom sample departures
2. Rest mode - display off
3. Ticker mode - candlestick chart (easter egg)
4. AP mode - WiFi setup credentials
5. WiFi connecting - connection status
6. Setup required - web UI address
7. API error - error message
8. No departures - info message
9. Normal operation (lowest) - real departures

## Configuration Constants

- **`MAX_DEPARTURES = 24`** ([DepartureData.h:11](../src/api/DepartureData.h#L11)) - Maximum cache size (hardcoded)
- **`DEPS_PER_STOP = 12`** ([DepartureData.h:10](../src/api/DepartureData.h#L10)) - Departures fetched per stop
- **`MAX_TEMP_DEPARTURES = 144`** (GolemioAPI/BvgAPI) - Collection buffer size (12 stops × 12 departures)
- **`config.numDepartures`** - User setting for display rows. Max depends on display size: `(panelRows * 32 / 8) - 1`, i.e. 3 on 128×32 (`panelRows=1`, default) or 7 on 128×64 (`panelRows=2`).

**Important:** `config.numDepartures` only controls how many rows to show on the LED matrix, not API fetch size. Both transit APIs (Prague Golemio and Berlin BVG) always fetch `DEPS_PER_STOP` (12) per stop for better caching and sorting. This simplifies the user experience - users don't need to understand API response sizes.

## Complete Pipeline Flow

```
┌──────────────────────────────────────────────────────────────────┐
│ 1. USER CONFIGURATION                                            │
│    config.city = "Prague" or "Berlin"  # Transit city selection  │
│    config.numDepartures = 2            # Show 2 display rows     │
│    config.pragueStopIds = "A,B"        # Query 2 Prague stops    │
│    config.berlinStopIds = "X,Y"        # Query 2 Berlin stops    │
└──────────────────────────────────────────────────────────────────┘
                              ↓
┌──────────────────────────────────────────────────────────────────┐
│ 2. API QUERIES (Always fetch DEPS_PER_STOP = 12 per stop)       │
│    TransitAPI::fetchDepartures() (GolemioAPI or BvgAPI)          │
│    loops through stops:                                          │
│    - Stop A: API call → 12 departures → tempDepartures[0-11]    │
│    - delay(1000)  # 1-second rate limiting                       │
│    - Stop B: API call → 12 departures → tempDepartures[12-23]   │
│    Total collected: 24 departures in temporary buffer            │
│    Buffer capacity: 144 (supports up to 12 stops)                │
└──────────────────────────────────────────────────────────────────┘
                              ↓
┌──────────────────────────────────────────────────────────────────┐
│ 3. SORT BY ETA (GolemioAPI.cpp:71)                              │
│    qsort(tempDepartures, 24, ..., compareDepartures)            │
│    All departures sorted by increasing ETA across all stops      │
│    Example sorted result:                                        │
│    [0] = Stop B, Line 7, ETA 2min                                │
│    [1] = Stop A, Line 31, ETA 5min                               │
│    [2] = Stop B, Line A, ETA 8min                                │
│    ... (21 more)                                                 │
└──────────────────────────────────────────────────────────────────┘
                              ↓
┌──────────────────────────────────────────────────────────────────┐
│ 4. COPY TO CACHE (GolemioAPI.cpp:81-88)                         │
│    Copy top MAX_DEPARTURES (24) from sorted temp to cache:       │
│    for (i = 0; i < tempCount && count < MAX_DEPARTURES; i++)    │
│        result.departures[count++] = tempDepartures[i];           │
│    Result:                                                       │
│    - result.departures[24] = top 24 soonest departures           │
│    - result.departureCount = up to 24                            │
│    - Each departure includes departureTime (Unix timestamp)      │
└──────────────────────────────────────────────────────────────────┘
                              ↓
┌──────────────────────────────────────────────────────────────────┐
│ 5. MAIN LOOP STORAGE (main.cpp)                                 │
│    Global cache in main.cpp:                                     │
│    - Departure departures[MAX_DEPARTURES] = cached results       │
│    - int departureCount = number of valid departures             │
│    Cache persists between API calls for ETA recalculation        │
└──────────────────────────────────────────────────────────────────┘
                              ↓
┌──────────────────────────────────────────────────────────────────┐
│ 6. REAL-TIME ETA UPDATES (Every 10 seconds, main.cpp)           │
│    recalculateETAs():                                            │
│    - For each departure in cache:                                │
│        eta = calculateETA(departure.departureTime)               │
│    - Remove stale departures (ETA < 0 or invalid)                │
│    - No API call needed - uses cached timestamps                 │
│    This keeps display fresh without hammering the API!           │
└──────────────────────────────────────────────────────────────────┘
                              ↓
┌──────────────────────────────────────────────────────────────────┐
│ 7. DISPLAY RENDERING (DisplayManager.cpp:345-414)               │
│    updateDisplay(..., departures, departureCount, numToDisplay)  │
│    - rowsToDraw = min(departureCount, numToDisplay, maxRows)    │
│    - rowsToDraw = min(24, 2, 3) = 2                             │
│    - for (i = 0; i < 2; i++): drawDeparture(i, departures[i])   │
│    Only first 2 departures shown on LED matrix (user setting)    │
│    Physical maximum is (panelRows*32/8)-1 rows: 3 on 128×32     │
│    (panelRows=1, default) or 7 on 128×64 (panelRows=2), with    │
│    the bottom row reserved for the date/time status bar         │
└──────────────────────────────────────────────────────────────────┘
```

## Design Rationale

### 1. Always Fetch DEPS_PER_STOP (12)
- Ensures good caching regardless of display setting
- Simplifies API logic - no user-dependent behavior
- Better sorting with more data points
- Users don't need to understand API response sizes

### 2. Large Temp Buffer (144)
- Supports up to 12 stops × 12 departures = 144 total
- Prevents data loss when querying multiple stops
- Memory cost: ~7KB (acceptable on ESP32 with ~200KB free)

### 3. Fixed Cache Size (24)
- Keeps "best" 24 departures after sorting
- Reasonable memory usage (~1.2KB)
- More departures than can be displayed for filtering flexibility and secondEta matching across multi-stop hubs

### 4. Display-Only User Control
- Maps directly to physical LED matrix rows (max 3 on 128×32, 7 on 128×64)
- Simple to understand: "How many rows to show?"
- No technical knowledge required

### 5. 10-Second ETA Recalculation
- Keeps display fresh without API calls
- Allows longer refresh intervals (up to 300s) to reduce load
- Filters out stale departures automatically

## Memory Allocation

### Data Structures

- **Temp Buffer**: `static Departure tempDepartures[144]` (~7KB)
  - Function-local static to avoid stack overflow
  - Located in `GolemioAPI::fetchDepartures()` and `BvgAPI::fetchDepartures()`
  - Allocated once at compile time

- **Cache**: `Departure departures[24]` (~1.2KB)
  - Global in `main.cpp`
  - Persists between API calls
  - Used for ETA recalculation

- **Display**: No departure storage
  - Receives pointer to cache
  - Zero memory overhead

**Total**: ~8KB for departure data structures

### Heap Usage

- JSON buffer: 12KB for Golemio API responses, 24KB for BVG API responses (DynamicJsonDocument)
- BVG API responses are more verbose (~1.7KB per departure vs Golemio's more compact format)
- Configuration: NVS flash storage (persistent across reboots)
- Typical free heap: ~200KB
- App partitions: two OTA slots of 2MB each (0x200000); exact RAM/flash utilization varies per build and hardware variant

## State Machine

The device operates in two modes:

### AP Mode (`apModeActive=true`)
- Creates WiFi network for setup
- DNS captive portal active
- Display shows credentials (SSID/password/IP)
- API calls disabled
- Web UI shows setup-focused interface

### STA Mode (`apModeActive=false`)
- Connects to configured WiFi
- Fetches departures every N seconds (configurable)
- ETA recalculation every 10 seconds
- Serves full web dashboard
- Demo mode available

### Demo Mode (`demoModeActive=true`)
- Pauses API polling and automatic display updates
- Shows user-configurable sample departures
- Available in both AP and STA modes
- Manually stopped via web interface or device reboot

### Rest Mode (`restModeActive=true`)
- Display cleared and brightness set to 0
- Triggered manually (web UI / REST API) or by scheduled time periods
- API polling continues (data stays fresh for when display resumes)
- Manual activation tracked separately (`restModeManual` flag)
- Scheduled activation follows configured periods (e.g., "23:00-07:00")

### State Transitions

```
Boot
  ↓
Try STA mode (20 attempts, ~10s)
  ↓
  ├─ Success → STA Mode
  │             ↓
  │           Connection loss?
  │             ↓
  │           Auto-reconnect (every 30s)
  │
  └─ Failure → AP Mode
               ↓
             Config saved?
               ↓
             Restart → Try STA mode
```

## Module Dependencies

Layered architecture with zero circular dependencies:

```
┌─────────────────────────────────────────────────────────┐
│ Layer 6: Application                                    │
│   main.cpp (orchestrates all modules, runtime API       │
│   selection based on config.city)                       │
└─────────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────────┐
│ Layer 5: Business Logic                                 │
│   TransitAPI (abstract), GolemioAPI, BvgAPI, MqttAPI,   │
│   WeatherAPI, TickerAPI, GitHubOTA                      │
└─────────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────────┐
│ Layer 4: Network Services                               │
│   WiFiManager, CaptivePortal, ConfigWebServer           │
│   OTAUpdateManager                                       │
└─────────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────────┐
│ Layer 3: Hardware Abstraction                           │
│   DisplayController, DisplayManager, DisplayColors,     │
│   TimeUtils, RestMode                                   │
└─────────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────────┐
│ Layer 2: Data Layer                                     │
│   AppConfig, DepartureData                              │
└─────────────────────────────────────────────────────────┘
                        ↓
┌─────────────────────────────────────────────────────────┐
│ Layer 1: Foundation                                     │
│   Logger, UTF-8 utilities (gfxlatin2, decodeutf8)       │
└─────────────────────────────────────────────────────────┘
```

### Key Patterns

- **Zero Circular Dependencies**: Lower layers never depend on higher layers
- **Single Responsibility**: Each module has one clear purpose
- **Callback Pattern**: Modules communicate upward via callbacks
  - Example: `ConfigWebServer` → `main.cpp` via `onSaveConfig` callback
- **Pure Data Structures**: Config passed as parameter, not stored in modules
- **Static Allocation**: No dynamic allocation in main loop for stability

### Module Communication

```
main.cpp
  ├─ Creates all modules and FreeRTOS tasks
  ├─ Registers callbacks
  ├─ Owns global state (departures array, mutexes)
  ├─ Runs loop() on Core 1: web server, ETA recalc, state management
  ├─ apiFetchTask() on Core 1: blocking HTTP calls, weather fetches
  └─ displayRenderTask() on Core 1: display rendering via notification

DisplayController
  ├─ State machine: decides what to display (9 priority levels)
  ├─ Delegates rendering to DisplayManager
  └─ No direct hardware access

DisplayManager
  ├─ Pure rendering layer for LED matrix
  ├─ Receives data as parameters (no caching)
  ├─ Handles UTF-8 to ISO-8859-2 conversion at render time
  └─ Accesses config pointer for color mapping and dual ETA mode

WiFiManager
  ├─ Manages WiFi connection
  ├─ Notifies main.cpp of state changes via flags
  └─ Provides status query methods

GolemioAPI / BvgAPI / MqttAPI
  ├─ Fetches departures via HTTP or MQTT
  ├─ Returns APIResult struct (no state stored)
  └─ Uses statusCallback for progress updates

ConfigWebServer
  ├─ Serves tabbed web interface (5 tabs, per-tab save)
  ├─ Handles demo mode and rest mode via callbacks
  └─ Communicates with main.cpp via callback pattern
```

## Multi-Stop Behavior

When multiple stop IDs are configured (comma-separated, max 12 stops):

1. **Query each stop individually** via separate API calls (always 12 departures per stop)
2. **Apply 1-second delay** between API calls to reduce server load and avoid rate limiting
3. **Collect in temp buffer** (capacity: 144 = 12 stops × 12 departures)
4. **Sort by ETA** (earliest departures first across all stops)
5. **Cache top 24** soonest departures with timestamps
6. **Display configured rows** (max 3 on 128×32, 7 on 128×64) on LED matrix
7. **Recalculate ETAs** every 10 seconds without additional API calls

This ensures you always see the **soonest** departures across all stops, regardless of which stop they come from.

### Rate Limiting

The 1-second delay between API calls (`delay(1000)` in [GolemioAPI.cpp:63](../src/api/GolemioAPI.cpp#L63)) prevents:
- HTTP 429 (Too Many Requests) errors
- Excessive load on Golemio API servers
- Connection timeouts from rapid requests

With 12 stops configured, a full query cycle takes ~12 seconds (plus network latency).

## Performance Characteristics

### API Call Timing
- **Single stop**: ~1-2 seconds (network latency)
- **Multiple stops**: ~1-2s per stop + 1s delay between stops
- **12 stops**: ~12-24 seconds total

### Display Update Timing
- **ETA recalculation**: <1ms (simple arithmetic on cached data)
- **Display render**: ~10-20ms (LED matrix DMA transfer)
- **Total refresh cycle**: ~30ms

### Memory Footprint
- **Stack usage**: Minimal (all large arrays are static or global)
- **Heap fragmentation**: None (no dynamic allocation in main loop)
- **Flash storage**: Configuration in NVS (~1KB)

## Debugging & Logging

### Debug Mode (Telnet)
When `config.debugMode = true`:
- Telnet server listens on port 23
- All `debugPrintln()` calls mirrored to telnet clients
- Memory usage logged at key points
- API responses logged with timestamps

### Serial Output
Always available (115200 baud):
- Boot sequence
- WiFi connection status
- API errors
- Configuration changes

### Memory Monitoring
Key checkpoints logged via `logMemory()`:
- `api_start` - Before API call
- `api_complete` - After processing response
- `display_update` - After display render

Use telnet to monitor memory in real-time:
```bash
telnet <device-ip> 23
```
