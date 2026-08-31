# SpojBoard - Smart Panel for Onward Journeys

An ESP32-based LED matrix display showing real-time transit departures. Supports multiple data sources: direct API integration (Prague Golemio, Berlin BVG) or MQTT for home automation integration.

**SPOJ** = **S**mart **P**anel for **O**nward **J**ourneys (also "spoj" = connection/service in Czech)
Standalone operation with web-based configuration, no external server required for direct API mode.

![SpojBoard Device](docs/images/spojboard-device.jpg)

**[📸 View More Screenshots](docs/SCREENSHOTS.md)**

## Documentation

- **[Wiring Guide](docs/WIRING.md)** - HUB75 pin mapping for all supported hardware variants
- **[MQTT Integration Guide](docs/MQTT.md)** - Home Assistant integration, custom data sources, and dual filtering setup
- **[Architecture & Data Flow](docs/ARCHITECTURE.md)** - Technical details, pipeline flow, and design rationale
- **[Font System](docs/FONTS.md)** - Czech character support and custom font creation guide
- **[Troubleshooting](docs/TROUBLESHOOTING.md)** - Common issues and solutions

## Features

- **Multi-Source Support**: Direct API (Prague Golemio, Berlin BVG) or MQTT for home automation integration (Home Assistant, custom sources)
- **Direct API Access**: Fetches departure data every 10-300 seconds (configurable)
- **MQTT Integration**: Request/response pattern with configurable JSON field mappings, dual ETA modes (timestamp/pre-calculated), and server-side aggregation - see [MQTT Guide](docs/MQTT.md)
- **Local ETA Updates**: Recalculates ETAs every 10 seconds from cached timestamps without additional API calls - reduces server load while keeping display fresh
- **Multi-Stop Support**: Query up to 12 stops, automatically sorted by departure time with 1s rate limiting; partial results paint as each stop responds, and a failed stop keeps its last-known departures
- **WiFi Setup Portal**: Automatic AP mode with captive portal when WiFi fails - no code editing needed
- **Dual ETA Display**: Optionally show next two departure times per line - see departure frequency at a glance (disabled by default)
- **Dual-Core Architecture**: FreeRTOS multi-task design - Core 0 handles WiFi interrupts, Core 1 runs display rendering, API fetching, and web server concurrently for responsive operation
- **Web Configuration**: Easy setup via tabbed web interface with city selector and per-tab save
- **Persistent Settings**: Configuration stored in ESP32 flash memory, survives reboots
- **Extended Character Support**: Custom 8-bit ISO-8859-2 fonts with automatic UTF-8 conversion (ž, š, č, ř, ň, ť, ď, ß, etc.)
- **Adaptive Font Rendering**: Automatically switches to condensed font for long destination names based on available space (accounting for ETA, platform, and dual ETA layout)
- **Optional Scrolling**: Enable scrolling for long destination names that exceed display width (disabled by default)
- **Selectable Panel Arrangement**: pick your panels in the web interface and reboot - no separate firmware. 128×32 (2× 64×32, up to 3 departure rows) by default; 128×64 (up to 7 rows) as either 4× 64×32 in a 2×2 grid, or 2× 64×64 chained - which also covers a **single 128×64 module**
- **Configurable Panel Wiring**: RGB channel order, panel driver chip and an optional custom GPIO pin map, all as settings. A panel whose colours come out wrong (orange looking pink, sky looking teal) is fixed in the UI rather than with a custom build. Includes a test pattern and a one-click restore of the built-in wiring, both reachable in setup mode
- **Ticker Mode** (easter egg): Candlestick price chart for stocks (e.g. AAPL) or crypto (e.g. BTC/USD) via the Twelve Data API
- **Headsign Shortening**: Automatic shortening of long Czech words (e.g., "Nádraží" → "Nádr.", "nádraží" → "nádr.") to maximize display space
- **Trip Filtering**: Configurable minimum departure time (default 3 minutes) to hide departures that are too soon to catch
- **AC Indicator**: Shows asterisk (*) for air-conditioned vehicles (Prague only)
- **Custom Line Colors**: Configure custom colors for specific lines via web interface with exact and pattern matching
- **Demo Mode**: Preview custom departure data on the display before API configuration - available in both setup and normal modes
- **Firmware Updates**: GitHub-based OTA updates with user confirmation - check for new releases directly from the web interface
- **Configuration Backup**: Download every setting as a JSON file and restore it - before a firmware update, to set up a second unit without retyping a 300-character API key, or just to keep a copy of a working setup. A restore only changes the settings the file actually contains, and nothing is written unless the whole file is valid. Panel arrangement and wiring restore only if you ask for them, so a backup from another unit cannot rearrange your display. **The file contains your WiFi password and API keys in plain text** - keep it private
- **Selective Reset**: A factory reset can optionally keep your WiFi credentials (so the device rejoins your network instead of starting a hotspot) and your panel arrangement and wiring (so the display comes back looking the same). Both off by default, and the reset asks you to type RESET
- **Optional Telnet Logging**: Remote debugging via telnet (port 23) when debug mode is enabled - mirrors all logs over WiFi
- **Weather Display**: Optional weather info in status bar showing temperature and condition icon from Open-Meteo API (free, no API key required)
- **Infotext Display**: Service alerts scroll through the status bar as a ticker, with datetime elements revealing char-by-char as the text passes. Manual infotext injection via web UI or REST API
- **Rest Mode**: Scheduled display power saving - configure time periods when the display turns off automatically

## Hardware

SpojBoard supports two hardware variants. The firmware configures the correct pin mapping for your board automatically - just flash the right firmware for your board.

**One firmware per board covers every panel arrangement.** Panel size and layout are settings, not separate downloads, so a release publishes exactly one file per board.

**A note on logic levels:** HUB75 panels are 5V logic devices, but the ESP32-S3 outputs 3.3V. Many panels work at 3.3V by accident (the input threshold is close enough), but this is out of spec and not guaranteed across all panel batches. Level shifters (3.3V → 5V) ensure reliable operation with any panel.

### Supported Hardware

| Variant | Board | Firmware File | Wiring | Level Shifting |
|---------|-------|---------------|--------|----------------|
| **MatrixPortal S3** | Adafruit MatrixPortal ESP32-S3 | `spojboard-matrixportal_s3-r*.bin` | Plug & play | Built-in (74AHCT245) |
| **ESP32-S3 N8R2** | Generic ESP32-S3 DevKitC (8MB Flash, 2MB PSRAM) | `spojboard-esp32_s3_n8r2-r*.bin` | Manual wiring | Not included |

### Option 1: MatrixPortal S3 (Recommended)

- **Board**: [Adafruit MatrixPortal ESP32-S3](https://www.adafruit.com/product/5778)
- **Display**: 2× HUB75 64×32 LED Matrix panels (128×32 total). Also supports 128×64 - 4× 64×32, 2× 64×64, or one 128×64 module - selected in the web interface
- **Power**: 5V 2A minimum (3A recommended) via USB-C
- **Wiring**: Built-in HUB75 connector - just plug in your panels
- **Level shifting**: Includes 2× 74AHCT245 level shifters, works reliably with all HUB75 panels

### Option 2: Generic ESP32-S3 N8R2

- **Board**: ESP32-S3-DevKitC-1 or similar (8MB flash minimum)
- **Display**: 2× HUB75 64×32 LED Matrix panels (128×32 total). Also supports 128×64 - 4× 64×32, 2× 64×64, or one 128×64 module - selected in the web interface
- **Power**: 5V 2A minimum (3A recommended) via screw terminals on panels
- **Wiring**: Standard HUB75 rainbow cable to GPIO pins

> **Warning:** Direct ESP32-S3 → HUB75 wiring runs at 3.3V, which is below the panels' 5V logic spec. It works with many panels but is not guaranteed across all manufacturers/batches. For reliable operation, add a [74AHCT245 level shifter](https://www.adafruit.com/product/1787) between the ESP32 and panel. See [ESP32-HUB75-MatrixPanel-DMA #134](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-DMA/issues/134) for details.

**→ See [Wiring Guide](docs/WIRING.md) for complete pinout and connection instructions.**

✅ **No code changes required** - the firmware automatically handles pin mapping differences between boards. Just use standard HUB75 cables.

### Power Supply Requirements

HUB75 LED matrix panels require external 5V power.

**Measured power consumption (real-world testing)**, on a 128×32 with typical content:
- **Normal operation** (brightness 90, default): ~0.3A average
- **Maximum brightness** (brightness 255): ~0.65A average, 0.7A peak
- **Transient spikes**: Up to 1.6A during WiFi connection, OTA updates, or boot

**Larger arrangements do not need a larger supply.** Panel current tracks the number of LEDs actually
*lit*, not the number of panels fitted, and a departure board is mostly dark — adding panels spreads
sparse text over more area rather than lighting more of it. The same recommendation below covers every
arrangement.

**Recommended power supply:**
- **5V 2A minimum** (provides 2.8× headroom over normal peaks, adequate for all conditions)
- **5V 3A recommended** (extra safety margin for simultaneous WiFi/OTA/display load)
- Undersized supplies usually show up as flickering, colour shifts at high brightness, or brownout reboots under load rather than as an outright failure to start

**Compatible supplies:**
- **USB-C**: Any quality 5V 2A+ phone charger or USB PD adapter (10W+)
- **Screw terminals**: 5V 2-3A regulated DC power supply (for generic ESP32 boards)

**Note:** Published HUB75 specifications often cite 5-6A at full brightness with all LEDs white, but SpojBoard's text-based display with typical content uses significantly less power in practice.

## Quick Start

### First Time Setup (AP Mode)

1. **Build & Flash the Firmware**
   ```bash
   pio run -t upload
   ```

2. **Connect to Setup AP**
   - The display will show WiFi credentials:
     ```
     WiFi Setup Mode
     SSID: SpojBoard-XXXX
     Pass: xxxxxxxx
     Go to: 192.168.4.1
     ```
   - Connect your phone/computer to this WiFi network
   - A captive portal should open automatically (or go to http://192.168.4.1)

3. **Configure via Web UI**
   - Enter your home WiFi credentials (required)
   - Select your data source:
     - **Prague**: Enter Golemio API key (get one at [api.golemio.cz/api-keys](https://api.golemio.cz/api-keys/))
     - **Berlin**: No API key required
     - **MQTT**: Configure broker and topics - see [MQTT Guide](docs/MQTT.md)
   - Enter your stop ID(s) - comma-separated for multiple stops (not used for MQTT)
   - Try the demo mode to preview the display before API setup
   - Click "Save & Connect to WiFi"

4. **Device Connects**
   - The device will restart and connect to your WiFi
   - Find its new IP address on the display or your router
   - Departures will start showing automatically

### If WiFi Fails

If the device can't connect to WiFi (wrong password, network down, etc.), it will automatically:
1. Stop trying after 20 attempts (~10 seconds)
2. Create a new AP with a **random password**
3. Display the credentials on the LED matrix
4. Wait for you to configure new WiFi settings

## Web Interface

The built-in web server provides a tabbed configuration interface with per-tab save:

- **Connection Tab** (📡): WiFi credentials, data source selector (Prague, Berlin, or MQTT)
- **Transit Data Tab** (🚇): API-specific configuration (dynamically shown based on selected city)
  - Prague: API key, stop IDs
  - Berlin: Stop IDs
  - MQTT: Broker, topics, field mappings - see [MQTT Guide](docs/MQTT.md)
  - Refresh interval, minimum departure time, number of departures
- **Display Tab** (🖥): Brightness, custom line colors with wildcard patterns and restore-to-defaults, dual ETA toggle, scrolling
- **Optional Tab** (⭐): Weather display, rest mode periods, debug mode (STA mode only)
- **Hardware Tab** (🔌): Panel arrangement, RGB channel order, panel driver chip, optional custom pin map, panel test pattern, restore built-in wiring. **Also shown in AP mode** - a panel blanked by a bad pin map is a plausible reason to be there. Arrangement lives here rather than on Display because it describes the panels you attached, not what is drawn on them, and both it and the wiring only take effect at boot - so changing them together costs one restart
- **System Tab** (⚙): System info, configuration backup, firmware updates (STA only), reboot and reset. **Also shown in AP mode** - restoring a backup and resetting are what you reach for when a device has dropped into setup mode, so they no longer sit behind the problem they fix
- **Action Bar**: Force refresh, demo mode, rest mode toggle (context-aware per mode)
- **Status Banners**: AP mode, demo mode, rest mode (manual/scheduled), API errors

### AP Mode vs Normal Mode

| Feature | AP Mode | Normal Mode |
|---------|---------|-------------|
| WiFi | Creates its own network | Connects to your network |
| API Calls | Disabled | Every 30-300s (configurable) |
| ETA Updates | N/A | Every 10s from cache |
| Display | Shows AP credentials | Shows departures |
| Web UI | Setup focused | Full dashboard |
| Demo Mode | Available | Available |
| Panel arrangement + wiring (Hardware tab) | Available | Available |
| Config backup / restore (System tab) | Available | Available |
| Reset, with optional keep-WiFi / keep-panel | Available | Available |
| Firmware updates (OTA) | Unavailable | Available |

## Getting Your Stop ID

Look up stop IDs with the station search in the docs at **[spojboard.xbco.cz/docs/station-search](https://spojboard.xbco.cz/docs/station-search/)** — no need to dig through the individual transit agency websites.

### Prague (Golemio API)
Find your stop and copy its GTFS ID (e.g., `U693Z2P`). You can enter multiple IDs separated by commas.

### Berlin (BVG API)
Find your stop and copy its numeric ID (e.g., `900013102`). You can enter multiple IDs separated by commas.

### MQTT (Home Assistant / Custom)
No stop IDs needed - configure your MQTT broker to aggregate and filter departures server-side. See **[MQTT Integration Guide](docs/MQTT.md)** for complete setup instructions.

## Display Layout

### Normal Mode
```
┌────────────────────────────────────┐
│ [31] Sídliště Řepy           12' │  Row 1
│ [7 ] Radlická                 5' │  Row 2
│ [A ] Depo Hostivař            2' │  Row 3
│ 08.02. Donnerstag  ☀ 15° 14:23  │  Row 4 (Date/Day/Weather/Time)
└────────────────────────────────────┘
```

### Dual ETA Mode (Optional)
When "Show multiple departure times" is enabled, each row shows the next two departures for the same line and destination:
```
┌────────────────────────────────────┐
│ [31] Nádraží            5'  32' │  Row 1
│ [A ] Letenské           2'  18' │  Row 2
│ [S9] Masarykovo        <1'  24' │  Row 3
│ 15.02. Saturday  ☀ 18°  14:35   │  Row 4
└────────────────────────────────────┘
```
Both ETAs are independently color-coded (red < 2min, yellow 2-5min, white > 5min). If no matching second departure exists, only the first ETA is shown. When a departure is more than 60 minutes away, the absolute departure time (e.g., "14:30") is shown instead of minutes, and dual ETA is suppressed for that row.

**Weather in Status Bar** (when enabled):
- Weather icon and temperature displayed between day name and time
- Temperature color-coded: cyan (cold), white (mild), yellow (warm), red (hot)
- Icon color indicates condition: yellow (sunny), white (cloudy), cyan (rain), blue (snow)

**Visual Design**:
- Line numbers displayed in their route color on uniform 18-pixel black background boxes (fits 1-3 characters)
- Route numbers are horizontally centered within their boxes for consistent alignment
- All destinations start at the same X position regardless of route length
- Adaptive font rendering: automatically switches to condensed font for long destinations based on available space
- Destination text dynamically truncated based on ETA display width and font choice to prevent overlap
- Status bar: numeric date (DD.MM.) + full localized day name (English/Czech/German) + optional weather + time. When service alerts (infotexts) are available, they scroll through as a ticker — datetime elements stay visible and reveal char-by-char as the text passes

### AP Setup Mode
```
┌────────────────────────────────────┐
│ WiFi Setup Mode                   │
│ SSID: SpojBoard-A1B2       │
│ Pass: k7m3p9x2                    │
│ Go to: 192.168.4.1                │
└────────────────────────────────────┘
```

## Color Coding

### Line Numbers (Default Colors)

These are the default colors (pre-configured, fully customizable via web interface):

- **Green**: Metro A
- **Yellow**: Default fallback for unmapped lines (`*???=YELLOW`)
- **Red**: Metro C
- **White**: Single-char lines (`*=WHITE`), trams 10-29 (`1*=WHITE`, `2*=WHITE`)
- **Purple**: T-buses 50-59 (`5*=PURPLE`), buses 100-299 (`1**=PURPLE`, `2**=PURPLE`)
- **Blue**: S-trains (`S*?=BLUE` — matches S1, S41, etc.)
- **Cyan**: Night lines (`9*?=CYAN` — matches 91-99, 900-999)

**Customizing Line Colors:**
- Configure specific colors for individual lines (e.g., `A=RED`)
- Use wildcards: `*` = required single-char position, `?` = optional position (must follow `*`):
  - `9*` matches exactly 2-char lines starting with 9 (91-99)
  - `S*?` matches 2 or 3-char lines starting with S (S1, S41)
  - `*???` matches any line of 1-4 chars (practical catch-all)
  - `4**` matches exactly 3-char lines starting with 4 (400-499)
- Three-pass priority: exact matches → fixed-length patterns (`*` only) → flexible patterns (with `?`)
- Invalid patterns (wildcards in prefix like `9*1`, `?` before `*`) are ignored
- Use "Restore Defaults" button in web UI to reset to built-in Prague transit defaults
- Available colors: RED, GREEN, BLUE, YELLOW, ORANGE, PURPLE, CYAN, WHITE

### ETA Colors
- **White**: > 5 minutes
- **Yellow**: 2-5 minutes
- **Red**: < 2 minutes
- **Orange**: Delayed

When dual ETA mode is enabled, both times are independently color-coded. The secondary ETA uses condensed font. Departures more than 60 minutes away display the absolute time (e.g., "14:30") instead of minutes.

## Weather Display

SpojBoard can optionally display current weather in the status bar using the free [Open-Meteo API](https://open-meteo.com/) (no API key required).

### Enabling Weather
1. Open the web interface at `http://[device-ip]/`
2. Enable "Weather Display" in the configuration form
3. Enter your location's GPS coordinates (latitude and longitude)
4. Optionally adjust the refresh interval (default: 15 minutes)
5. Save configuration

### Finding Your Coordinates
- Use [Google Maps](https://maps.google.com): Right-click any location → coordinates are shown
- Common cities: Prague (50.0755, 14.4378), Berlin (52.5200, 13.4050)

### Weather Icons
| Condition | Icon | Color |
|-----------|------|-------|
| Clear/Sunny | ☀ | Yellow |
| Cloudy | ☁ | White |
| Fog | 🌫 | Purple |
| Rain/Drizzle | 🌧 | Cyan |
| Snow | ❄ | Blue |
| Thunderstorm | ⛈ | Red |

### Temperature Colors
- **Blue**: Below 8°C (cold)
- **White**: 8-16°C (mild)
- **Yellow**: 17-25°C (warm)
- **Red**: Above 25°C (hot)

## Rest Mode

Rest mode automatically turns off the display during configured time periods to save power and reduce light pollution at night. It can also be controlled manually via the web interface or REST API.

### Configuration
1. Open the web interface at `http://[device-ip]/`
2. Enter time periods in the "Rest Mode" field
3. Format: `HH:MM-HH:MM` (24-hour format)
4. Multiple periods: separate with commas

### Examples
| Configuration | Behavior |
|---------------|----------|
| `23:00-07:00` | Off from 11 PM to 7 AM |
| `00:00-06:00` | Off from midnight to 6 AM |
| `22:00-06:00,12:00-13:00` | Off overnight and during lunch |
| (empty) | Rest mode disabled (default) |

### Manual Control

You can also enable/disable rest mode manually from the web interface:

1. Open the web interface at `http://[device-ip]/`
2. Click "Enable Rest Mode" button in Actions section
3. Display turns off immediately (regardless of scheduled periods)
4. Click "Disable Rest Mode" to resume normal operation

**Manual vs Scheduled:**
- **Manual activation**: Takes priority, overrides schedule, indicated as "Rest Mode Active (Manual)" in status
- **Scheduled activation**: Follows configured time periods, indicated as "Rest Mode Active (Scheduled)" in status
- **Status indicator**: Shows whether rest mode is "Manual" or "Scheduled" in the dashboard

**Use cases:**
- Quick display shutoff without editing time periods
- Integration with home automation (REST API endpoint: `POST /rest-mode`)
- Testing rest mode before configuring schedule

**Notes:**
- Cross-midnight periods work correctly (e.g., "22:00-06:00")
- API polling continues during rest mode (data stays fresh)
- Display resumes automatically when rest period ends

## Troubleshooting

Having issues? See the **[Troubleshooting Guide](docs/TROUBLESHOOTING.md)** for solutions to common problems including WiFi connection issues, API errors, display problems, and firmware update issues.

## Firmware Updates

SpojBoard supports two methods for updating firmware:

### Method 1: GitHub Updates (Recommended)

Check for and install new releases directly from GitHub:

1. **Open the web interface** at `http://[device-ip]/`
2. **Click "Check for Updates"** in the Actions section
3. **Review the update** (version, release notes, file size)
4. **Click "Download & Install"** if update is available
5. **Wait for download** - progress shown on LED matrix
6. **Device reboots automatically** with new firmware

**Features:**
- ✅ Automatic version checking
- ✅ Displays release notes before installing
- ✅ Secure HTTPS download with MD5 validation
- ✅ Progress display on LED matrix
- ✅ Safe - failed update doesn't brick the device

**Requirements:**
- Device must be connected to WiFi (not in AP mode)
- Internet access to github.com

### Method 2: Manual Upload

Upload a firmware .bin file manually:

1. **Get the firmware file** for your hardware variant:
   - MatrixPortal S3: `spojboard-matrixportal_s3-r6-abc12345.bin`
   - ESP32-S3 N8R2: `spojboard-esp32_s3_n8r2-r6-abc12345.bin`
2. **Open web interface** and click "Update Firmware"
3. **Select .bin file** and click "Upload Firmware"
4. **Wait for upload** - progress shown on screen
5. **Device reboots** after successful upload

**Use cases:**
- Installing custom firmware builds
- Offline updates
- Development testing

### Update Security

All firmware updates (both methods):
- ❌ **Disabled in AP mode** (prevents unauthorized access)
- ✅ **MD5 validation** (corrupted firmware rejected)
- ✅ **HTTPS only** for GitHub downloads
- ✅ **Separate OTA partition** (failed update doesn't affect running firmware)
- ✅ **User confirmation required** (no automatic updates)



## Technical Details

For detailed information about the dual-core architecture, data flow pipeline, memory allocation, and design decisions, see **[Architecture & Data Flow](docs/ARCHITECTURE.md)**.

### AP Mode Credentials
- SSID: `SpojBoard-XXXX` (XXXX = last 4 chars of MAC address)
- Password: 8 random alphanumeric characters (regenerated each time)
- IP: `192.168.4.1`

### Captive Portal
The device implements captive portal detection for:
- Android (`/generate_204`, `/gen_204`)
- iOS/macOS (`/hotspot-detect.html`)
- Windows (`/ncsi.txt`, `/connecttest.txt`)
- Firefox (`/success.txt`)

### Memory Usage
- Per-API JSON buffers: Golemio 12KB, BVG 32KB read cap (filtered to an 8KB parse doc), MQTT 8KB
- Configuration stored in NVS flash
- Typical free heap: ~200KB
- Two 2MB OTA app partitions (app0/app1); exact RAM/flash figures vary per hardware variant and build

### Multi-Stop Behavior
When multiple stop IDs are configured (comma-separated, max 12 stops), the system:
1. Queries each stop individually (Prague fetches 12 departures per stop; Berlin fetches 10 to stay within its response-size limit)
2. Applies 1-second delay between API calls to reduce server load and avoid rate limiting
3. Merges each stop's results into a running buffer — and if one stop's fetch fails, that stop keeps its last-known departures instead of going blank
4. Sorts by ETA (earliest departures first across all stops)
5. Caches the top 24 soonest departures with timestamps for ETA recalculation
6. Displays the configured number of rows (up to 3 on 128×32, up to 7 on 128×64) on the LED matrix, painting partial results as soon as the first stop responds
7. Recalculates ETAs every 10 seconds from cached timestamps without additional API calls

This ensures you always see the **soonest** departures across all your stops, regardless of which stop they come from. The board fetches more departures than it shows for better caching and sorting — the user only controls how many rows to display on the LED matrix (up to 3 on 128×32, up to 7 on 128×64). The 10-second ETA recalculation keeps the display fresh without hammering the API, allowing longer refresh intervals (up to 300s) during peak times.

### Destination String Shortening
To maximize display space on the LED matrix, long destination names are automatically shortened before display:
- **Czech** (Prague): "Nádraží" → "Nádr." (uppercase), "nádraží" → "nádr." (lowercase), "Sídliště" → "Sídl.", "Nemocnice" → "Nem."
- **German** (Berlin): "Hauptbahnhof" → "Hbf", "Bahnhof" → "Bhf", "(Berlin)" → "(B)"

Combined with adaptive font rendering (condensed font for destinations >16 chars), even very long station names fit comfortably on the display. Shortening is applied before UTF-8 to ISO-8859-2 conversion, ensuring diacritics are preserved correctly.

## Fonts & Character Support

SpojBoard uses custom 8-bit ISO-8859-2 fonts with automatic UTF-8 conversion to display special characters (**Czech**: ž, š, č, ř, ň, ť, ď, ú, ů, á, é, í, ó, ý; **German**: ß, ẞ, ä, ö, ü, Ä, Ö, Ü) from transit APIs.

For complete details on the font system, UTF-8 conversion, and creating custom fonts, see **[Font System Documentation](docs/FONTS.md)**.

## License

This project is licensed under the **GNU General Public License v3.0 (GPL-3.0)**.

This means you are free to:
- ✅ Use this code for personal or commercial purposes
- ✅ Modify and distribute the code
- ✅ Create derivative works

**However, you must**:
- 📝 Disclose the source code of any derivative works
- 📝 License derivative works under GPL-3.0
- 📝 Include copyright and license notices

See the LICENSE file for full details.

## Credits

- [Golemio API](https://api.golemio.cz) - Prague open data platform
- [ESP32-HUB75-MatrixPanel-DMA](https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-DMA) - Display driver
- [Adafruit GFX Library](https://github.com/adafruit/Adafruit-GFX-Library) - Graphics primitives and font rendering
- [Adafruit GFX Font Customiser by tchapi](https://tchapi.github.io/Adafruit-GFX-Font-Customiser/)
- [Michel Deslierres](https://sigmdel.ca/michel/program/misc/gfxfont_8bit_en.html) - Original UTF-8 to ISO-8859-2 conversion code
- [Petr Brouzda - fontconvert8-iso8859-2](https://github.com/petrbrouzda/fontconvert8-iso8859-2) - Font conversion tool and UTF-8 conversion implementation
- [Claude Code](https://claude.ai/code) - AI-assisted development and code refinement
- [DepartureMono font family](https://departuremono.com/) - Custom 8-bit GFXfonts with ISO-8859-2 character support