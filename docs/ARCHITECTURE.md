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
│   Orchestrates per-stop fetch + accumulator,    │
│   blocking HTTP calls (200-2000ms), publishes   │
│   departures via mutex, sleeps 100ms            │
│                                                 │
│ Arduino loop()       [Priority 1]               │
│   Web server, ETA recalculation, state mgmt    │
└─────────────────────────────────────────────────┘
```

### Thread Safety

Five mutexes protect shared data with short lock durations (~1ms); all are created in `AppState::createMutexes()` (see `core/AppState`):

- **`displayMutex`** - Protects `DisplayUpdateRequest` struct (display task <-> loop/API)
- **`apiDataMutex`** - Protects `departures[]`/`departureCount`, `weatherData`, `infoText`, `tickerData`, `stopName`, `apiError`, `apiErrorMsg`, `awaitingDepartures` (API task <-> loop)
- **`displayHwMutex`** - Serializes all HUB75 DMA-buffer writes: the render task's `render()` vs. direct draws from `onAPIStatus` (API-task ctx) / `onDemoStop` (loop ctx). Leaf lock.
- **`signalMutex`** - Serializes `signalDisplayUpdate()` against itself (called from both loop and `apiFetchTask`, uses static snapshot buffers). Outer lock — wraps the `apiDataMutex`+`displayMutex` acquisitions.
- **`configMutex`** - Guards `config` between its writer (`onConfigSave`) and `apiFetchTask`'s per-iteration snapshot. Leaf lock.

Lock order (acyclic, no deadlock): `signalMutex` → `apiDataMutex` → `displayMutex` (sequential); `displayHwMutex` and `configMutex` are leaves.

**Data snapshot pattern**: Data is copied under mutex, then processed without locks. Rendering and HTTP calls never hold a mutex.

> **Implementation note (TA-0225):** the task bodies and shared state were extracted out of `main.cpp` into a `core/` layer — `AppState` (shared state + mutexes), `AppCallbacks` (web/API callbacks), `DisplayBridge` (`signalDisplayUpdate` + `displayRenderTask`), `TransitOrchestrator` (`apiFetchTask` + accumulator + `recalculateETAs`), `AppRuntime` (setup/loop glue). Where this document says "main.cpp" for the fetch pipeline / accumulator / global cache, the code now lives in the corresponding `core/` module; `main.cpp` is now just the `setup()`/`loop()` skeleton.

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

- **`MAX_DEPARTURES = 24`** ([DepartureData.h:11](../src/api/DepartureData.h#L11)) - Per-stop `StopResult` buffer size and the final cache cap. Larger than any display so the secondEta matcher has depth within a stop.
- **`DEPS_PER_STOP = 12`** ([DepartureData.h:10](../src/api/DepartureData.h#L10)) - Departures requested per stop (Golemio) and the accumulator/temp-buffer sizing factor.
- **`BVG_MAX_RESULTS = 10`** ([BvgAPI.h](../src/api/BvgAPI.h)) - BVG clamps its own `results=` request to this. A busy hub's BVG payload is >2.7 KB/departure, so 12 would overflow the 32 KB read cap and truncate (`IncompleteInput`). Golemio's compact payload has no such limit and requests the full `DEPS_PER_STOP`.
- **`MAX_TEMP_DEPARTURES = 144`** (MqttAPI only) - MQTT's server-aggregate scratch buffer (`DEPS_PER_STOP × 12`). Golemio/BVG no longer use a per-client temp buffer — they write straight into the per-stop `StopResult.departures` (capped at `MAX_DEPARTURES`); the cross-stop accumulator lives in `apiFetchTask`.
- **`config.numDepartures`** - User setting for display rows. Max depends on display size: `(panelRows * 32 / 8) - 1`, i.e. 3 on 128×32 (`panelRows=1`, default) or 7 on 128×64 (`panelRows=2`).

**Important:** `config.numDepartures` only controls how many rows to show on the LED matrix, not API fetch size. Golemio always requests `DEPS_PER_STOP` (12) per stop; BVG requests `BVG_MAX_RESULTS` (10) to stay within its read cap. Fetching more than displayed gives the sorter and secondEta matcher more to work with. Users don't need to understand API response sizes.

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
│ 2. PER-STOP FETCH (apiFetchTask orchestrates, not the client)   │
│    stopCount = transitAPI->getStopCount(config)                  │
│    for s in 0..stopCount-1:                                      │
│      StopResult sr = transitAPI->fetchStop(config, s)           │
│        # Golemio: results=12 | BVG: results=10 (read-cap safe)  │
│        # writes straight into sr.departures[MAX_DEPARTURES]     │
│      if sr.hasError: keep this stop's previous rows (keep-stale) │
│      else: evict acc entries tagged s, append sr's fresh rows    │
│      delay(1000)  # 1-second inter-stop rate limiting            │
└──────────────────────────────────────────────────────────────────┘
                              ↓
┌──────────────────────────────────────────────────────────────────┐
│ 3. ACCUMULATOR SORT (main.cpp, per stop)                        │
│    Persistent: AccEntry acc[DEPS_PER_STOP*12] (Departure + tag) │
│    qsort(acc, accCount, ..., compareAccEntry)  # by ETA         │
│    The stopIndex tag is bound INTO AccEntry so it co-permutes    │
│    with its departure — a parallel tag array would desync and    │
│    mis-target the per-stop eviction (was a duplicate-row bug).   │
└──────────────────────────────────────────────────────────────────┘
                              ↓
┌──────────────────────────────────────────────────────────────────┐
│ 4. PUBLISH SNAPSHOT (publishDepartureSnapshot)                  │
│    Filter stale / below-minDepartureTime, cap to MAX_DEPARTURES, │
│    attach secondEta (gated by sourceStopId), copy into the       │
│    shared departures[] under apiDataMutex, signal display.       │
│    Progressive: during initial fill the board publishes after    │
│    each stop (partial data shows ~1s sooner); in steady state it │
│    replaces silently and publishes once at cycle end.            │
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

### 1. Fetch More Than Displayed (per-API)
- Golemio requests `DEPS_PER_STOP` (12); BVG requests `BVG_MAX_RESULTS` (10) to stay under its 32 KB read cap
- Ensures good caching regardless of display setting and gives the sorter/secondEta matcher depth
- Per-API request count is decoupled from the shared buffer sizing — one overloaded knob can't satisfy both a compact and a verbose backend

### 2. Single Cross-Stop Accumulator (`apiFetchTask`)
- `AccEntry acc[DEPS_PER_STOP × 12]` persists across cycles, tagged by stop index for keep-stale-per-stop eviction
- Replaced the three per-client `tempDepartures[144]` buffers (one per API, all linked but only one active) — reclaimed ~32 KB internal RAM (TA-0190)
- MQTT still needs its own `tempDepartures[144]` (its server-side aggregate must be sorted before handing back one `StopResult`) plus an 8 KB `responseBuffer` — but these are **lazily heap-allocated on the first MQTT `fetchStop()`**, so a Prague/Berlin device never pays them (~27 KB static + ~8 KB heap reclaimed; TA-0190)

### 3. Fixed Cache Size (24)
- `MAX_DEPARTURES` caps both the per-stop `StopResult` and the published snapshot
- Reasonable memory usage (~3 KB per `StopResult`)
- Larger than any display so the **per-stop** secondEta matcher has depth. secondEta is gated by `sourceStopId` — a line+destination only departs from one stop, so matches are never conflated *across* stops (the old "across multi-stop hubs" framing is explicitly **not** wanted)

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

- **Accumulator**: `static AccEntry acc[DEPS_PER_STOP × 12]` in `apiFetchTask` (~20KB)
  - One cross-stop buffer (was three per-client `tempDepartures[144]`, ~32 KB reclaimed — TA-0190)
  - `AccEntry` = `Departure dep` + `int stopIndex` (the stop tag travels through the ETA `qsort`)
  - Function-local static (`.bss`), never on the task stack
  - MQTT additionally needs `tempDepartures[144]` + an 8 KB `responseBuffer`, but **lazily heap-allocated only when the active city is MQTT** (`MqttAPI::ensureInitialized`) — Prague/Berlin keep ~27 KB static + ~8 KB heap free; measured boot `MaxBlock` +41 KB on 4-panel

- **Per-stop result**: `StopResult` (`Departure departures[MAX_DEPARTURES]`, ~3KB)
  - Filled by `fetchStop()`; the orchestrator merges it into the accumulator

- **Cache**: `Departure departures[24]` (~3KB)
  - Global in `main.cpp`
  - Persists between API calls
  - Used for ETA recalculation

- **Display**: No departure storage
  - Receives pointer to cache
  - Zero memory overhead

### Heap Usage

- JSON buffer: 12KB for Golemio responses; BVG reads up to 32KB but parses through an ArduinoJson `Filter` into an 8KB `DynamicJsonDocument` (keeps only the ~6 fields used)
- BVG API responses are verbose (>2.7KB per departure at busy hubs vs Golemio's compact format) — the filter keeps RAM low on 4-panel builds, and BVG clamps `results=10` so the raw response stays under the 32 KB read cap (12 would truncate → `IncompleteInput`)
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
  ├─ Implements getStopCount() + fetchStop(index)
  ├─ Returns one StopResult per stop (no state stored; hasError = fail vs empty)
  └─ Uses statusCallback for progress updates

ConfigWebServer
  ├─ Serves tabbed web interface (5 tabs, per-tab save)
  ├─ Handles demo mode and rest mode via callbacks
  └─ Communicates with main.cpp via callback pattern
```

## Multi-Stop Behavior

When multiple stop IDs are configured (comma-separated, max 12 stops), `apiFetchTask` drives the loop (the API client just serves one stop at a time via `fetchStop`):

1. **Query each stop individually** via `fetchStop(config, s)` (Golemio 12, BVG 10 departures per stop)
2. **Apply 1-second delay** between API calls to reduce server load and avoid rate limiting
3. **Merge into the accumulator**: on success, evict that stop's previous rows and append the fresh ones; on failure, **keep the stop's stale rows** so a single failed fetch never blanks it
4. **Sort the accumulator by ETA** (earliest first across all stops; the stop tag co-permutes)
5. **Publish top 24** soonest departures with timestamps — **progressively** during the initial fill (partial board shows ~1 s sooner), silently-then-once in steady state
6. **Display configured rows** (max 3 on 128×32, 7 on 128×64) on LED matrix
7. **Recalculate ETAs** every 10 seconds without additional API calls (re-sorts and re-attaches secondEta)

This ensures you always see the **soonest** departures across all stops, regardless of which stop they come from — while a transient failure of one stop leaves the others (and that stop's last-known rows) intact.

### Rate Limiting

The 1-second delay between per-stop API calls (`delay(1000)` in the `apiFetchTask` orchestration loop, [main.cpp](../src/main.cpp); hoisted out of the API clients during the per-stop refactor) prevents:
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

### Debug Mode
When `config.debugMode = true`, the transit clients and `readHttpResponse()` emit
verbose diagnostics on Serial: per-chunk HTTP read progress, raw API payload sizes
and the first few parsed departures. It gates volume only — it is not a separate
log sink. (A telnet mirror on port 23 existed until r9 and was removed along with
the `ESPTelnet` dependency.)

### Serial Output
Always available (115200 baud):
- Boot sequence
- WiFi connection status
- API errors
- Configuration changes

### Memory Monitoring
Key checkpoints logged via `logMemory()` (printed as `MEM@<label>`):
- `boot` / `display_init` - Startup, before and after the DMA framebuffer allocation
- `post_fetch` - After the first departures fetch (the HTTPS handshake peak)
- `weather_start` / `weather_complete` - Around the weather fetch (tightest contiguous-block point)

Watch these on the serial console:
```bash
pio device monitor
```
