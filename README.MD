# SpojBoard - Smart Panel for Onward Journeys

An ESP32-based LED matrix display showing real-time transit departures. Supports multiple data sources: direct API integration (Prague Golemio, Berlin BVG) or MQTT for home automation integration.

**SPOJ** = **S**mart **P**anel for **O**nward **J**ourneys (also "spoj" = connection/service in Czech)
Standalone operation with web-based configuration, no external server required for direct API mode.

![SpojBoard Device](docs/images/spojboard-device.jpg)

**[📸 View More Screenshots](docs/SCREENSHOTS.md)**

## Documentation

- **[MQTT Integration Guide](docs/MQTT.md)** - Home Assistant integration, custom data sources, and dual filtering setup
- **[Architecture & Data Flow](docs/ARCHITECTURE.md)** - Technical details, pipeline flow, and design rationale
- **[Font System](docs/FONTS.md)** - Czech character support and custom font creation guide
- **[Hardware Guide for Generic ESP32-S3](docs/GENERIC_ESP32S3.md)** - Alternative hardware setup
- **[Troubleshooting](docs/TROUBLESHOOTING.md)** - Common issues and solutions

## Features

- **Multi-Source Support**: Direct API (Prague Golemio, Berlin BVG) or MQTT for home automation integration (Home Assistant, custom sources)
- **Direct API Access**: Fetches departure data every 10-300 seconds (configurable)
- **MQTT Integration**: Request/response pattern with configurable JSON field mappings, dual ETA modes (timestamp/pre-calculated), and server-side aggregation - see [MQTT Guide](docs/MQTT.md)
- **Local ETA Updates**: Recalculates ETAs every 10 seconds from cached timestamps without additional API calls - reduces server load while keeping display fresh
- **Multi-Stop Support**: Query up to 12 stops simultaneously (144 departure buffer), automatically sorted by departure time with 1s rate limiting
- **WiFi Setup Portal**: Automatic AP mode with captive portal when WiFi fails - no code editing needed
- **Web Configuration**: Easy setup via built-in web interface with city selector
- **Persistent Settings**: Configuration stored in ESP32 flash memory, survives reboots
- **Extended Character Support**: Custom 8-bit ISO-8859-2 fonts with automatic UTF-8 conversion (ž, š, č, ř, ň, ť, ď, ß, etc.)
- **Adaptive Font Rendering**: Automatically switches to condensed font for long destination names (>16 chars), fitting up to 23 characters on screen
- **Optional Scrolling**: Enable scrolling for long destination names that exceed display width (disabled by default)
- **Headsign Shortening**: Automatic shortening of long Czech words (e.g., "Nádraží" → "Nádr.", "nádraží" → "nádr.") to maximize display space
- **Trip Filtering**: Configurable minimum departure time (default 3 minutes) to hide departures that are too soon to catch
- **AC Indicator**: Shows asterisk (*) for air-conditioned vehicles (Prague only)
- **Custom Line Colors**: Configure custom colors for specific lines via web interface with exact and pattern matching
- **Demo Mode**: Preview custom departure data on the display before API configuration - available in both setup and normal modes
- **Firmware Updates**: GitHub-based OTA updates with user confirmation - check for new releases directly from the web interface
- **Optional Telnet Logging**: Remote debugging via telnet (port 23) when debug mode is enabled - mirrors all logs over WiFi
- **Weather Display**: Optional weather info in status bar showing temperature and condition icon from Open-Meteo API (free, no API key required)
- **Rest Mode**: Scheduled display power saving - configure time periods when the display turns off automatically

## Hardware

### Recommended Setup

- **Controller**: Adafruit MatrixPortal ESP32-S3
- **Display**: 2× HUB75 64×32 LED Matrix panels (128×32 total resolution)
- **Power Supply**: Minimum 5V 3A switching power supply
  - Can also be powered by a 5V 3A USB-C charger: wire the power cables from the displays to MatrixPortal's 5V/GND terminals and power through its USB-C port.

### Alternative: Generic ESP32-S3

SpojBoard firmware can also run on generic ESP32-S3 development boards with custom wiring. See [Generic ESP32-S3 Guide](docs/GENERIC_ESP32S3.md) for details.

**Caveats:**
- Requires manual GPIO pin mapping configuration
- Needs custom wiring (no plug-and-play connector)
- May require level shifters for some displays
- Cost savings: ~$10-20 per unit

### Power Supply Requirements

HUB75 LED matrix panels require external 5V power.

**Typical power consumption (displaying text):**
- Two 64×32 panels (128×32): ~1-2A at typical brightness showing colored text
- ESP32-S3: ~500mA peak during WiFi transmission
- **Total**: ~2-3A typical

**Note:** Peak consumption with all LEDs at full white would be ~5A, but this never occurs during normal operation with text display.

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

The built-in web server provides:

- **Status Dashboard**: WiFi status, API status, memory usage
- **Configuration Form**:
  - WiFi credentials
  - Data source selector (Prague, Berlin, or MQTT)
  - API key (Prague only)
  - MQTT broker, topics, and field mappings (MQTT only) - see [MQTT Guide](docs/MQTT.md)
  - Stop ID(s) - comma-separated for multiple stops (max 12, not used for MQTT)
  - Refresh interval (10-300 seconds)
  - Number of departures to display (1-3 rows)
  - Minimum departure time (0-30 minutes) - filter during ETA recalculation
  - Display brightness (0-255)
  - Custom line colors with position-based wildcard patterns (e.g., "9*" for 2-digit, "4**" for 3-digit)
  - Debug mode (enables telnet logging on port 23)
  - Scrolling mode (scroll long destination names)
  - Weather display (enable/disable, latitude/longitude, refresh interval)
  - Rest mode (time periods when display turns off, e.g., "23:00-07:00")
- **Actions**:
  - Force refresh
  - Display demo (preview custom departures)
  - Update firmware (manual .bin upload)
  - Check for updates (GitHub releases)
  - Reboot device
  - Factory reset

### AP Mode vs Normal Mode

| Feature | AP Mode | Normal Mode |
|---------|---------|-------------|
| WiFi | Creates its own network | Connects to your network |
| API Calls | Disabled | Every 30-300s (configurable) |
| ETA Updates | N/A | Every 10s from cache |
| Display | Shows AP credentials | Shows departures |
| Web UI | Setup focused | Full dashboard |
| Demo Mode | Available | Available |

## Getting Your Stop ID

### Prague (Golemio API)
1. Visit [data.pid.cz/stops/json/stops.json](https://data.pid.cz/stops/json/stops.json)
2. Search for your stop name (Ctrl+F)
3. Find the `gtfsIds` value (e.g., `U693Z2P`)
4. You can enter multiple IDs separated by commas

### Berlin (BVG API)
1. Visit [v6.bvg.transport.rest](https://v6.bvg.transport.rest/)
2. Use the `/locations` endpoint to search for your stop
3. Find the stop ID (numeric, e.g., `900013102`)
4. You can enter multiple IDs separated by commas

### MQTT (Home Assistant / Custom)
No stop IDs needed - configure your MQTT broker to aggregate and filter departures server-side. See **[MQTT Integration Guide](docs/MQTT.md)** for complete setup instructions.

## Display Layout

### Normal Mode
```
┌────────────────────────────────────┐
│ [31] Sídliště Řepy           12' │  Row 1
│ [7 ] Radlická                 5' │  Row 2
│ [A ] Depo Hostivař            2' │  Row 3
│ Mon| Feb 15    ☀ 18°    14:35    │  Row 4 (Date/Time + Weather)
└────────────────────────────────────┘
```

**Weather in Status Bar** (when enabled):
- Weather icon and temperature displayed between date and time
- Temperature color-coded: blue (cold), white (mild), yellow (warm), red (hot)
- Icon color indicates condition: yellow (sunny), white (cloudy), cyan (rain), blue (snow)

**Visual Design**:
- Line numbers displayed in their route color on uniform 18-pixel black background boxes (fits 1-3 characters)
- Route numbers are horizontally centered within their boxes for consistent alignment
- All destinations start at the same X position regardless of route length
- Adaptive font rendering: automatically switches to condensed font for destinations longer than 16 characters
- Destination text dynamically truncated based on ETA display width and font choice to prevent overlap

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

These colors are used by default and can be customized via the web interface:

- **Green**: Metro A
- **Yellow**: Metro B, Default fallback for all unmapped lines
- **Red**: Metro C
- **White**: Trams 1-29 (e.g., 2, 5, 7, 10, 12, 15, 17, 22)
- **Purple**: 2-digit T-buses 50-59 & 3-digit buses 100-299 (e.g., 59, 100, 102, 200)
- **Blue**: S-trains (S1, S2, S3, etc.)
- **Cyan**: Night lines 91-99, 900-999

**Customizing Line Colors:**
- Configure specific colors for individual lines (e.g., "A=RED")
- Use position-based wildcard patterns with asterisks as position placeholders:
  - "9*" matches 2-digit lines (91-99)
  - "95*" matches 3-digit lines (950-959)
  - "4**" matches 3-digit lines (400-499)
  - "C***" matches 4-digit lines (C000-C999)
- Exact matches take priority over patterns
- Invalid patterns (leading asterisks "***", non-trailing asterisks "9*1") are ignored
- Unmapped lines use the hardcoded defaults above
- Available colors: RED, GREEN, BLUE, YELLOW, ORANGE, PURPLE, CYAN, WHITE

### ETA Colors
- **White**: > 5 minutes
- **Yellow**: 2-5 minutes
- **Red**: < 2 minutes
- **Orange**: Delayed

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

Rest mode automatically turns off the display during configured time periods to save power and reduce light pollution at night.

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

1. **Get the firmware file** (e.g., `spojboard-r2-abc12345.bin`)
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

For detailed information about the data flow pipeline, memory allocation, and design decisions, see **[Architecture & Data Flow](docs/ARCHITECTURE.md)**.

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
- JSON buffer: 8KB for API responses
- Configuration stored in NVS flash
- Typical free heap: ~200KB
- RAM usage: 21.4% (70KB used of 327KB)
- Flash usage: 94.7% (1.24MB used of 1.31MB)

### Multi-Stop Behavior
When multiple stop IDs are configured (comma-separated, max 12 stops), the system:
1. Queries each stop individually via separate API calls (always fetches 12 departures per stop)
2. Applies 1-second delay between API calls to reduce server load and avoid rate limiting
3. Collects all departures in temporary buffer (capacity: 144 total = 12 stops × 12 departures)
4. Sorts by ETA (earliest departures first across all stops)
5. Caches the top 12 soonest departures with timestamps for ETA recalculation
6. Displays the configured number of rows (1-3) on the LED matrix
7. Recalculates ETAs every 10 seconds from cached timestamps without additional API calls

This ensures you always see the **soonest** departures across all your stops, regardless of which stop they come from. The API always fetches 12 departures per stop for caching and sorting - the user only controls how many rows (1-3) to display on the LED matrix. The 10-second ETA recalculation keeps the display fresh without hammering the API, allowing longer refresh intervals (up to 300s) during peak times.

### Destination String Shortening
To maximize display space on the 128×32 LED matrix, long Czech destination names are automatically shortened before display (Prague only):
- "Nádraží" → "Nádr." (uppercase)
- "nádraží" → "nádr." (lowercase)
- "Sídliště" → "Sídl."
- "Nemocnice" → "Nem."

Combined with adaptive font rendering (condensed font for destinations >16 chars), even very long station names fit comfortably on the display. Shortening is applied before UTF-8 to ISO-8859-2 conversion, ensuring Czech diacritics are preserved correctly. Berlin station names are displayed as-is from the API.

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