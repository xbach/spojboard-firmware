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

**Tab values:** `connection`, `transit`, `display`, `optional`, `hardware`, `system`, `all`

`system` is reachable (it is the active tab when you save from there) but runs no parser - that tab
holds actions, not settings. `all` runs every parser **only outside AP mode**, where the page really
did render every tab; an AP-mode save posts `all` while showing only Connection and Hardware, so
running the rest would read each absent checkbox as unchecked. See `network/TabDispatch.h`.

| Tab | Key Fields |
|-----|-----------|
| `connection` | `ssid`, `password`, `city` (Prague/Berlin/MQTT) |
| `transit` | `refresh` (10-300s), `min_dep_time` (0-30), `prague_stops`, `berlin_stops`, `api_key`, MQTT fields |
| `display` | `brightness` (0-255), `num_deps` (1 to 3 or 7 depending on arrangement), `language`, `show_platform`, `scroll_enabled`, `show_multi_times`, `line_color_map`, `platform_symbol_map` |
| `optional` | `debug_mode`, `weather_enabled`, `weather_lat`, `weather_lon`, `weather_refresh` (5-120 min), `rest_periods` |
| `hardware` | `panel_geom` (1-3, see below), `hw_rgb_order` (0-5), `hw_driver` (0-5), `hw_custom_pins` (checkbox), `hw_r1` … `hw_clk` (GPIO 0-48) |

Every range above is enforced in exactly one place — `configClamp()` in `src/config/ConfigJson.h` —
which the boot load, this save path and the JSON importer all call. The per-parser copies that used
to do it drifted: `weather_refresh` rendered as `min=5 max=120` while saves were silently narrowed
to 10-60, so the form advertised a range it refused to keep. Values outside a range are clamped, not
rejected; the response is still `200`.

**`panel_geom`** names the panel ARRANGEMENT, not the pixel size — two of the three are 128×64.
It sits on the **hardware** tab with the wiring: both describe the attached panels rather than what
is drawn on them, both are read only at boot, so changing both costs one reboot instead of two. It is
therefore also settable in **AP mode**, where the hardware tab renders and the display tab does not.

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
Factory reset: erases all NVS settings and reboots into AP mode. Available in AP mode.

**Requires `confirm=RESET`.** The confirmation is enforced here, not only in the UI — a destructive
endpoint whose sole protection is a dialog in the page that calls it is protected against nothing.

Two optional keep-flags, both defaulting to off, so an unqualified call is still a full reset:

| Param | Effect |
|---|---|
| `keep_wifi=1` | preserve `wifiSsid` / `wifiPass`. The device rejoins the network on the same address instead of starting a hotspot — AP mode is entered when `connectSTA()` *fails*, so keeping the credentials is the whole mechanism |
| `keep_display=1` | preserve the panel arrangement **and** wiring (`dispGeom`, `panelRows`, `hwCustom`, `hwPins`, `hwRgbOrder`, `hwDriver`), so the display comes back looking the same |

One flag covers arrangement and wiring together, unlike the import's two: there is no
board-portability question on a reset, both simply describe the panel in front of you.

```bash
# full factory reset
curl -X POST -d 'confirm=RESET' http://<device>/clear-config

# wipe settings but stay reachable and keep the panel working
curl -X POST -d 'confirm=RESET&keep_wifi=1&keep_display=1' http://<device>/clear-config
```

`configured` is cleared either way, so the device shows **Setup Required** until a transit source is
configured again — reachable over the network rather than a hotspot when `keep_wifi=1`.

Without it, `400`:

```json
{"ok": false, "error": "Factory reset requires confirm=RESET"}
```

`hw_variant` survives any reset: it records which board this is, and a reset does not change that.
Everything else goes unless a keep-flag says otherwise — **including panel arrangement and wiring**,
so on 64px panels or a custom pin map the display comes back wrong until it is set again on the
Hardware tab (reachable in AP mode for exactly this reason) or `keep_display=1` is passed.

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

### Configuration Backup

Both routes are available in **AP mode**: restoring a backup and resetting to factory are what a
user reaches for when a device has dropped into setup mode.

#### `GET /config-export`
Download every setting as JSON. Sent with
`Content-Disposition: attachment; filename="spojboard-<deviceCode>-config.json"`, where the device
code is the same one used by the hostname and AP SSID.

> **The file contains `wifiPassword`, `pragueApiKey`, `mqttPassword` and `tickerApiKey` in
> plaintext.** That is deliberate — it is what makes the file a working backup and a way to clone a
> second unit — but it means the file is a secret. Do not attach it to a bug report.

The document is stamped with `schema`, `release` and `board`. Typical size ~1.9KB; ~3.6KB with every
string field full.

#### `POST /config-import`
Apply a configuration document, then reboot.

The body is the **raw JSON document** — `Content-Type: application/json`, read from
`arg("plain")` — not a form and not multipart. A urlencoded 4KB config roughly doubles on the wire
and makes the device build three large heap buffers to decode it again; a raw body arrives once.

Opt-ins ride in the query string, both defaulting off:

| Param | Effect |
|---|---|
| `geometry=1` | also restore the panel arrangement (`dispGeom`). Describes the **panels**, so it applies across board types |
| `wiring=1` | also restore the pin map, channel order and driver. Describes **this controller's GPIOs**, so it is refused on a board mismatch even when asked |

```bash
curl -X POST -H 'Content-Type: application/json' \
     --data-binary @spojboard-9B9D2C-config.json \
     'http://<device>/config-import?geometry=1'
```

```json
{"ok": true, "fields": 41, "boardMismatch": false, "wiringRefused": false,
 "geometryApplied": true, "wiringApplied": false, "fileBoard": "matrixportal_s3"}
```

**Any key the file omits keeps the value already on the device**, so a backup taken before a
firmware update still restores cleanly after one, and an export from a build that predates a field
cannot blank it. Unknown keys are ignored rather than rejected, which is what lets an older firmware
read a newer export.

Nothing is written unless the whole document is valid — the import is applied to a copy and only
persisted on success, so a truncated or malformed file leaves NVS byte-identical. Errors are `400`:

```json
{"ok": false, "error": "That file is not valid JSON, or is truncated or too large."}
```

A body over 8192 bytes is `413`. Every imported value passes the same clamps a boot-time load
applies, so no import can produce a configuration the device would have rejected at startup.

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
  "assetUrl": "https://github.com/...",
  "currentDisplay": "2x32",
  "options": [
    {
      "name": "spojboard-matrixportal_s3-r3-abc123.bin",
      "url": "https://github.com/...",
      "display": "",
      "size": 1048576
    }
  ]
}
```

`currentDisplay` is the panel-arrangement token this device is configured for (`2x32`, `4x32`,
`2x64`), or empty if unknown. `options[]` lists **every** asset in the release that matches this
board, geometry-tokenised builds before bare ones. One entry means there is nothing to choose and
the UI installs it directly; more than one means the release ships per-arrangement builds and the
user picks, preselected on `currentDisplay`.

The firmware deliberately does not choose for you: a binary runs at any arrangement, because the
arrangement is runtime config. Board mismatch is the only hard rejection, and it is enforced twice -
at selection and again in `downloadAndInstall`.

**Response (no update):**
```json
{"available": false}
```

#### `POST /download-update`
Download and install firmware from the latest GitHub release. Device reboots after successful install.

**Request body (required):**
```json
{
  "assetUrl": "https://github.com/...",
  "expectedSize": 1048576
}
```

Both come from one entry of `/check-update`'s `options[]`. A missing or zero value for either
returns `400 {"success": false, "error": "Invalid request parameters"}` - this is the route by
which the user's choice of build is made, so there is no default.

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
