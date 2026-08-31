# Changelog

All notable changes to SpojBoard firmware will be documented in this file.

## [Unreleased]

### Added
- **Panel wiring is now configurable** on a new Hardware tab: RGB channel order, panel driver chip, and an optional custom pin map. A panel whose colours come out wrong — orange looking pink, sky looking teal — is now a setting rather than a custom firmware build
- A **test pattern** button draws three bars labelled R, G and B, so a channel swap takes two seconds to spot instead of guesswork
- A **restore built-in wiring** button that reboots back to the factory pin map. Both it and the Hardware tab work in setup (AP) mode, so a blank panel can always be recovered without a USB cable

### Changed
- **Panel arrangement is chosen in the settings again**, and now names the panels instead of counting rows: 2x 64x32 chained (128x32), 4x 64x32 in a 2x2 grid (128x64), or 2x 64x64 chained (128x64). Both 128x64 options are the same pixel size but different hardware, so the list says which panels each one means
- A **single 128x64 module** is supported by the 2x 64x64 setting — the two are identical as far as the display driver is concerned, so no separate firmware is needed
- Existing settings carry over: a display set to 128x32 stays 128x32, and one set to 128x64 becomes the 4-panel grid, which is what that option has always meant
- The **panel arrangement setting moved to the Hardware tab**, next to the wiring. Both describe the panels you attached rather than what is drawn on them, and both only take effect at boot, so changing them together now costs one restart instead of two. It is also reachable during setup (AP) mode, where the Display tab is not shown

### Fixed
- Saving from the setup (AP) portal no longer silently switches off settings it never showed you. Platform symbols, destination scrolling, dual departure times, debug logging and weather were all turned off by any save made during setup, because the form reported sending every tab while only showing two
- Saving a wiring change now actually restarts the device. It used to save the setting, announce a restart, and not perform one — so the new channel order or pin map sat unapplied until the next power cycle
- The restart screen names the reason it is restarting. Anything that was not a transit-provider change previously claimed "WiFi Network Changed" and showed an SSID nobody had touched

## [r9] - 2026-08-22

### Fixed
- Weather now shows rain, not snow, during rain showers
- Long text is measured correctly again, fixing scroll positions that could jump a line
- A display that fails to start no longer stops WiFi, the web interface and updates from working, so a board with a panel fault stays reachable
- Update checks no longer fail on releases that offer several downloads

### Added
- Choose which panel layout to install when a release offers more than one, preselected to match the current setting
- Desktop test suite covering firmware-file naming and update parsing

### Changed
- The board now appears on your network as `spojboard-9B9D2C` instead of `esp32s3-9B9D2C`, so it is recognisable in a router's device list; the name is shown on the System tab and matches the setup network name the board creates
- Firmware build IDs are now the git commit they were built from, so a board reports exactly which source it runs; builds from uncommitted changes are labelled `-dirty`
- Exact toolchain and library versions are pinned, so a given release always rebuilds to the same firmware
- Removed telnet logging; the debug-mode setting now controls detailed logging on the serial console

## [r8] - 2026-06-24

### Added
- Progressive multi-stop display: departures paint as each stop responds, instead of waiting for the slowest stop
- Fetch more departures per stop (Prague) for better "next departure" (secondary ETA) coverage

### Changed
- Secondary ETA now matches only departures from the same stop
- Lower memory use (lazy MQTT buffers + per-stop refactor), freeing internal RAM on 4-panel displays
- Minor DepartureMono font glyph adjustments

### Fixed
- Remove duplicate departure rows (which also made the secondary ETA mirror the main one)
- Work with older Golemio API keys that include an email address (#5)
- Keep the last good weather reading during a brief network hiccup instead of blanking it, and retry failed fetches sooner
- A stop that fails to refresh now keeps its last-known departures instead of going blank
- Parse Berlin (BVG) departures reliably at busy hubs

## [r7] - 2026-06-23


### Changed
- Shorten the datetime delay before the first infotext scroll-in for a snappier status bar

### Fixed
- Parse large BVG responses correctly and raise HTTP timeouts to 15s to prevent dropped fetches
- Resolve 4-panel (128x64) bootloop by dropping the PSRAM workaround
- Brighten the secondary ETA gray on the 4-panel 5-bit display for better legibility
- Handle quotes and control characters safely in infotext JSON
- Center the status message above the status bar

## [r6] - 2026-05-12

### Added
- Infotext scrolling in status bar with datetime alternation, including Golemio service alerts and a manual override test page
- Candlestick chart ticker mode via Twelve Data API (stocks and crypto)
- 128x64 display support with VirtualMatrixPanel, web UI display size selector, and dynamic numDepartures max
- Configurable line color defaults with `?` optional positional wildcard
- Absolute departure time display (HH:MM) for distant departures beyond 60 minutes
- Claude-powered changelog generation tooling for releases

### Changed
- Refactored display rendering to use DisplayLayout struct, replacing hardcoded row, width, Y-axis, and status bar magic numbers
- Hide non-connection tabs and action buttons while in AP mode
- Adjusted medium/condensed font selection thresholds

### Fixed
- 4-panel memory issues and config save crash on 128x64 builds
- DST transitions now use a POSIX TZ string for correct CET/CEST handover
- Buffer safety hardened, data race resolved, and heap fragmentation reduced
- Destination overflow when dual ETA is enabled alongside a single-ETA row
- Zero-pad hour in absolute departure time
- Scrolling destination clear bounds corrected

## [r5] - 2026-02-09

### Added
- Dual ETA display showing next departure for same line+destination
- Platform-to-arrow directional symbols (configurable per platform/stop ID)
- Multi-hardware variant support (MatrixPortal S3 + generic ESP32-S3 N8R2)
- Manual rest mode toggle via web UI
- Departures debug list in web config
- "Loading Departures..." screen on boot and rest mode exit (replaces misleading "No Departures")
- Help text for line color wildcard pattern syntax
- Rest mode period input validation (HH:MM-HH:MM format)

### Changed
- Redesigned web configuration interface with tabbed layout
- Per-tab config save (only sends active tab fields)
- Full day names and numeric dates in status bar
- Display rendering moved to CPU core 1 for smoother updates
- Display state machine centralized in DisplayController
- Departure sort: secondary sort by destination (was line name)
- Cold temperature color changed from blue to cyan

### Fixed
- Line color config lost on save (stale JS serialization overwrote with wrong selectors)
- Timezone parsing issues for some departures
- XSS vulnerabilities, thread safety, HTTP limits, config validation
- Rest mode display clearing via signalDisplayUpdate
- Font descender artifacts removed

## [r4] - 2026-01-20

### Added
- Weather display with Open-Meteo API integration (icon and temperature on status bar)
- Rest mode for scheduled display power saving with REST API control endpoint
- MQTT integration for self-hosted transit data (see `docs/MQTT.md`)
- Configurable destination scrolling option for long names (default: off)
- Code formatting config (clang-format and editorconfig)

### Performance
- Chunked HTTP reading for Prague and Berlin APIs (reduced memory usage)

### Fixed
- Line number preformatting for consistent display alignment

## [r3] - 2026-01-10

### Added
- **Demo Mode**: Standalone demo mode with customizable sample departures
  - Web interface to edit sample departures (line, destination, ETA, A/C status)
  - Available in both AP and STA modes
- **Smart ETA Updates**: 10-second ETA recalculation from cached timestamps
  - Reduces API calls by 6x while keeping display fresh
  - Allows longer refresh intervals (up to 300s)
- **Telnet Logging**: Remote debugging via telnet (port 23)
  - Enable via debugMode config field in settings
- **Extended Departure Buffer**: Increased from 6 to 12 departures
  - Provides 8-12 minute buffer during peak times
- **Condensed Font Support**: Automatic font switching for long destinations
  - DepartureMonoCondensed5pt8b for destinations >16 chars (23 char capacity)
- **API Retry Logic**: Improved error handling and user feedback
- **Factory Reset**: Reset to defaults via settings page
- **Custom Line Colors**: User-configurable color mapping system
  - Pattern matching with trailing asterisk (e.g., "9*=CYAN")

### Changed
- **API Optimization**: Always fetch MAX_DEPARTURES (12) per stop for optimal caching
  - Increased JSON buffer to 12KB for busy stops
- **Web UI Improvements**
- **Display Improvements**
- **Logging**: All Serial.println() converted to debugPrintln() for consistent logging

### Fixed
- **OTA Handler**: Split handleUpdateUpload into separate progress/complete handlers
- **Stack Overflow**: Made tempDepartures static (~2KB moved off stack)
- **API Query**: minDepartureTime now queried in API, removed unnecessary filtering in parser
- **Font Metrics**: Fixed xAdvance values in DepartureMono5pt8b

## [r2] - 2026-01-09

### Fixed
- Enable HTTP redirect following for GitHub asset downloads

## [r1] - Initial Release

Initial public release of SpojBoard firmware.
