# SpojBoard Development Backlog

This file tracks planned features, improvements, and technical debt for the SpojBoard project.

## Status Legend
- `[ ]` - Not started
- `[~]` - In progress
- `[x]` - Completed
- `[?]` - Needs clarification/discussion

---

## High Priority Features

### Status Bar Improvements
- [x] Extend day of week to full word (no abbreviations)
- [x] Shorten date format to numeric (e.g., "15.02" instead of "08.Jan")

### Display Configuration
- [ ] Add option to remove status bar entirely
  - [ ] Enable 4th line for departures when status bar disabled
  - [ ] Automatically disable weather display when status bar removed
  - [ ] Add `config.showStatusBar` boolean setting
  - [ ] Update web UI with toggle option
  - [ ] Update `DisplayManager` to support 4-row departure mode

### Station Search
- [ ] Implement station ID search via API
  - [ ] Search by station name (user input)
  - [ ] Return matching station codes and names
  - [ ] Add search UI to web interface
  - [ ] Support Prague (Golemio API)
  - [ ] Support Berlin (BVG API)
  - [ ] MQTT: document search approach (server-side responsibility)

### Departure Time Display
- [x] Add option to show next two departure times per line (dual ETA mode)
  - [x] Condensed font for dual ETAs, adaptive destination font based on available space
  - [x] Update API parsers to collect multiple times per line
  - [x] Update `DisplayManager` rendering logic
  - [x] Per-tab config save in web UI

### Platform Display Customization
- [ ] Custom platform override
  - [ ] Configurable platform symbol per platform value
  - [ ] Add `config.platformSymbolMap` string field (format: "PLT_VALUE=SYMBOL,...")
  - [ ] Example: "B=>,A=<,C=^,D=v"
  - [ ] Will use a custom font for the symbols to allow for 8 direction symbols
  - [ ] Update web UI for platform symbol configuration
  - [ ] Update `DisplayManager` to use custom symbols
  - [ ] Document symbol usage

---

## Medium Priority Features

### Web Interface
- [ ] Add inline station search to configuration form
- [ ] Improve color picker UI for line colors
- [ ] Add preview mode for configuration changes (before save)

### Display Enhancements
- [ ] Add transition animations between updates
- [ ] Support custom fonts (user-uploadable)
- [ ] Add brightness schedule (auto-dim at night)

### Network Features
- [ ] mDNS support (discover as spojboard.local)
- [ ] Fallback to cached data on network loss
- [ ] Configurable WiFi reconnection strategy

---

## Low Priority / Nice to Have

### Hardware Support
- [ ] Support for different panel sizes (64x64, 128x64)
- [ ] Hardware button support for brightness/mode toggle
- [ ] Support for additional hardware variants

### API Features
- [ ] Additional transit API integrations (suggest: GTFS-RT)
- [ ] Multi-city display (split screen for 2 cities)
- [ ] Traffic delay severity indicators

### Developer Experience
- [ ] Add unit tests for core modules
- [ ] CI/CD improvements (automated testing)
- [ ] Simulation mode (run without hardware)

---

## Technical Debt

- [x] Refactor `ConfigWebServer` HTML generation (moved to `src/network/web/` dedicated files)
- [ ] Extract font rendering logic from `DisplayManager`
- [ ] Improve error recovery in API clients
- [ ] Add memory usage monitoring/logging
- [ ] Review and optimize JSON buffer sizes

---

## Documentation

- [x] Update power supply requirements in README/docs
  - Updated measured data: 5V, brightness 90: 0.3A avg, brightness 255: 0.65A avg / 0.7A peak, transient spikes: 1.6A
  - Recommendations: 5V 2A minimum, 5V 3A recommended
  - USB-C focused, future-safe approach
  - Updated README.md (lines 60, 67, 74-83) and WIRING.md (lines 94-101)
- [ ] Add hardware assembly guide with photos
- [ ] Document custom font creation process
- [ ] Add troubleshooting guide
- [ ] Create video tutorial for first-time setup

---

## Notes

- Items can be moved between priority levels as needs change
- Add implementation notes under each item as work progresses
- Link to issues/PRs when work begins: `[#123](link)`
- Update status as work progresses
