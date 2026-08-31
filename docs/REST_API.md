# REST API Reference

SpojBoard exposes a local HTTP API on port 80 for configuration and control.

> **Note:** Some endpoints are blocked in AP mode (captive portal). These are marked with *STA only*.

## Endpoints

### Dashboard & Configuration

#### `GET /`
Serves the main dashboard page with device status and tabbed configuration form.

#### `POST /save`
Save configuration settings. Uses form-encoded data with a `tab` parameter to scope which settings are saved.

**Content-Type:** `application/x-www-form-urlencoded`

**Tab values:** `connection`, `transit`, `display`, `optional`, `hardware`, `all`

| Tab | Key Fields |
|-----|-----------|
| `connection` | `ssid`, `password`, `city` (Prague/Berlin/MQTT) |
| `transit` | `refresh` (10-300s), `min_dep_time` (0-30), `prague_stops`, `berlin_stops`, `api_key`, MQTT fields |
| `display` | `panel_geom` (1-3, see below), `brightness` (0-255), `num_deps` (1 to 3 or 7 depending on arrangement), `language`, `show_platform`, `scroll_enabled`, `show_multi_times`, `line_color_map`, `platform_symbol_map` |
| `optional` | `debug_mode`, `weather_enabled`, `weather_lat`, `weather_lon`, `weather_refresh`, `rest_periods` |
| `hardware` | `hw_rgb_order` (0-5), `hw_driver` (0-5), `hw_custom_pins` (checkbox), `hw_r1` … `hw_clk` (GPIO 0-48) |

**`panel_geom`** names the panel ARRANGEMENT, not the pixel size — two of the three are 128×64:

| Value | Panels | Pixels |
|-------|--------|--------|
| `1` | 2× 64×32 chained | 128×32 |
| `2` | 4× 64×32, 2×2 grid | 128×64 |
| `3` | 2× 64×64 chained, **or one 128×64 module** | 128×64 |

**`hw_rgb_order`**: `0` RGB (standard cable), `1` RBG (MatrixPortal with 64×32 panels), `2` GRB, `3` GBR, `4` BRG, `5` BGR.
**`hw_driver`**: `0` SHIFTREG (default), `1` FM6124, `2` FM6126A, `3` ICN2038S, `4` MBI5124, `5` DP3246.

A custom pin map that fails validation (duplicate pins, GPIO 22-25 or above 48, or the SPI-flash pins 26-32) is stored so the form shows it back, but is **ignored at boot** in favour of the built-in map — the driver is never handed a map validation rejects.

**Response (no restart needed):**
```json
{"success": true, "message": "Configuration saved"}
```

**Response (restart needed):** HTML page with restart animation (triggers when WiFi, city or panel arrangement changed, when any `hardware` field changed, or in AP mode).

---

### Device Control

#### `POST /refresh`
Force an immediate API fetch. Redirects to `/` after triggering.

#### `POST /reboot`
Restart the device. Returns an HTML status page before rebooting.

#### `POST /clear-config`
Factory reset: erases all NVS settings and reboots into AP mode.

#### `POST /hw-test`
Draw the panel colour test: three bars labelled R, G and B. If a letter sits on the wrong colour, the RGB channel order is wrong. The pattern stays on the panel until something else repaints it. Available in AP mode.

```json
{"ok": true}
```

Returns `503` with `{"ok": false, "error": "not wired"}` if the display callback is unavailable.

#### `POST /reset-display-pins`
Restore the built-in panel wiring — pin map, channel order and driver chip — and reboot. The recovery path for a panel left dark by a wrong-but-valid pin map, and deliberately **not** gated on the stored map being invalid. Available in AP mode, so it never requires a working panel.

```json
{"ok": true, "rebooting": true}
```

---

### Rest Mode

#### `POST /rest-mode` *STA only*
Toggle display rest mode (screen off, API polling continues).

**Request:**
```json
{"enabled": true}
```

**Response:**
```json
{"success": true, "restMode": true}
```

**Errors:**
- `400` — Missing `enabled` field
- `403` — Not available in AP mode

---

### Demo Mode

#### `GET /demo`
Serves the demo configuration page.

#### `POST /start-demo`
Activate demo mode with custom departure data.

**Request:**
```json
{
  "departures": [
    {
      "line": "22",
      "destination": "Nadrazi Holesovice",
      "eta": 5,
      "platform": "A",
      "hasAC": true,
      "secondEta": 12
    }
  ]
}
```
Up to 3 departures. `line`, `destination`, `eta` are required; `platform`, `hasAC`, `secondEta` are optional.

**Response:**
```json
{"success": true, "message": "Demo mode activated"}
```

#### `POST /stop-demo`
Deactivate demo mode and resume normal operation.

---

### Infotext (Service Alerts)

#### `GET /infotext`
Serves the infotext test page.

#### `POST /set-infotext`
Set a manual infotext message to scroll in the status bar. Blocks API infotext updates until cleared.

**Request:**
```json
{"text": "Your message here"}
```

**Response:**
```json
{"success": true}
```

#### `POST /clear-infotext`
Clear manual infotext and re-enable API-sourced alerts.

#### `GET /current-infotext`
Get current infotext state.

**Response:**
```json
{
  "active": true,
  "manual": true,
  "text": "Current scrolling text"
}
```

---

### Departures Data

#### `GET /departures`
Serves an HTML page displaying cached departure data.

#### `GET /departures-data`
Returns cached departures as JSON (used for AJAX refresh).

**Response:**
```json
{
  "count": 2,
  "max": 24,
  "deps": [
    {
      "l": "22",
      "d": "Nadrazi Holesovice",
      "e": 5,
      "s": 12,
      "p": "A",
      "ac": true,
      "dl": false,
      "dm": 0,
      "sid": "U693Z2P"
    }
  ]
}
```

| Field | Description |
|-------|-------------|
| `l` | Line number |
| `d` | Destination |
| `e` | ETA (minutes) |
| `s` | Secondary ETA (-1 if none) |
| `p` | Platform |
| `ac` | Air conditioning |
| `dl` | Is delayed |
| `dm` | Delay minutes |
| `sid` | Source stop ID |

---

### Ticker Mode (Easter Egg)

#### `GET /ticker` *STA only*
Serves the ticker configuration page.

#### `POST /start-ticker` *STA only*
Save settings and activate ticker mode. Uses form-encoded data.

**Parameters:**
- `ticker_symbol` (required) — e.g. `AAPL`, `BTC/USD`
- `ticker_api_key` (required) — Twelve Data API key
- `ticker_interval` — `1h`, `4h`, or `1day`
- `ticker_refresh` — refresh interval in seconds (120-600)

**Response:**
```json
{"success": true, "message": "Ticker mode activated"}
```

#### `POST /stop-ticker`
Stop ticker mode and return to departure display.

#### `POST /ticker-mode` *STA only*
Toggle ticker mode via JSON (same interface as rest-mode).

**Request:**
```json
{"enabled": true}
```

**Response:**
```json
{"success": true, "tickerMode": true}
```

---

### OTA Firmware Updates *STA only*

#### `GET /update`
Serves the manual firmware upload form.

#### `POST /update`
Upload a `.bin` firmware file (multipart form upload). Streams directly to OTA partition.

#### `GET /check-update`
Check GitHub for available firmware updates.

**Response (update available):**
```json
{
  "available": true,
  "releaseNumber": 3,
  "releaseName": "r3",
  "releaseNotes": "Bug fixes",
  "fileName": "spojboard-matrixportal_s3-r3-abc123.bin",
  "fileSize": 1048576,
  "assetUrl": "https://github.com/..."
}
```

**Response (no update):**
```json
{"available": false}
```

#### `POST /download-update`
Download and install firmware from the latest GitHub release. Device reboots after successful install.

---

### Captive Portal Routes

These routes handle automatic captive portal detection by various operating systems. All redirect to the configuration page.

| Route | OS |
|-------|-----|
| `GET /generate_204` | Android |
| `GET /gen_204` | Android |
| `GET /hotspot-detect.html` | iOS / macOS |
| `GET /library/test/success.html` | iOS / macOS |
| `GET /ncsi.txt` | Windows |
| `GET /connecttest.txt` | Windows |
| `GET /redirect` | Generic |
| `GET /success.txt` | Firefox |

### Catch-All

Any unmatched route returns a `302` redirect to `/` (or `http://192.168.4.1/` in AP mode).
