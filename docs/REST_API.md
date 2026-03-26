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

**Tab values:** `connection`, `transit`, `display`, `optional`, `all`

| Tab | Key Fields |
|-----|-----------|
| `connection` | `ssid`, `password`, `city` (Prague/Berlin/MQTT) |
| `transit` | `refresh` (10-300s), `min_dep_time` (0-30), `prague_stops`, `berlin_stops`, `api_key`, MQTT fields |
| `display` | `brightness` (0-255), `num_deps` (1-3), `language`, `show_platform`, `scroll_enabled`, `show_multi_times`, `line_color_map`, `platform_symbol_map` |
| `optional` | `debug_mode`, `weather_enabled`, `weather_lat`, `weather_lon`, `weather_refresh`, `rest_periods` |

**Response (no restart needed):**
```json
{"success": true, "message": "Configuration saved"}
```

**Response (restart needed):** HTML page with restart animation (triggers when WiFi, city changed, or in AP mode).

---

### Device Control

#### `POST /refresh`
Force an immediate API fetch. Redirects to `/` after triggering.

#### `POST /reboot`
Restart the device. Returns an HTML status page before rebooting.

#### `POST /clear-config`
Factory reset: erases all NVS settings and reboots into AP mode.

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
