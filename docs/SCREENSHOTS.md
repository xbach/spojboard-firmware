# SpojBoard Screenshots

Visual overview of SpojBoard hardware and web interface.

## Hardware

### Device in Action

![SpojBoard Device](images/spojboard-device.jpg)

The SpojBoard displaying real-time Prague transit departures on a 128×32 HUB75 LED matrix panel.

## Adaptive Font Rendering

### Condensed Font for Long Destinations

![Condensed Font Example](images/condensedFont.jpeg)

Automatically switches to condensed font when destination names exceed 16 characters, fitting up to 23 characters on screen while maintaining readability.

## Demo Mode

### Anything Goes if it Fits

![Bober Express](images/demoMode.jpeg)

## Web Interface

The web interface was redesigned with a tabbed layout for better organization. Configuration is split across five tabs: Connection, Transit Data, Display, Optional, and System.

### Main Configuration Screen

![Settings Screen 1](images/settings01.png)

> **Note:** Screenshots may show the older single-page layout. The current interface uses a tabbed design with per-tab save functionality.

Web-based configuration interface showing:
- **Connection tab**: WiFi settings and data source selector (Prague/Berlin/MQTT)
- **Transit Data tab**: API-specific settings (API keys, stop IDs, refresh interval)
- **Display tab**: Brightness, line colors, dual ETA toggle, scrolling options
- **Optional tab**: Weather display, rest mode periods, debug mode
- **System tab**: Device info, firmware updates, actions (refresh, reboot, factory reset)

### Configuration Details

![Settings Screen 2](images/settings02.png)

Additional configuration options including:
- Display brightness slider (0-255)
- Custom line colors with wildcard pattern support (e.g., "9*=CYAN" for all night lines)
- Action buttons for refresh, firmware update, and reboot
- GitHub-based OTA update checker
- Factory reset option

## Dual ETA Display

When "Show multiple departure times" is enabled in the Display tab, each row shows the next two departures for the same line and destination side by side:
```
[31] Nádraží            5'  32'
[A ] Letenské           2'  18'
```
Both ETAs are independently color-coded by urgency. Useful for high-frequency services to see departure intervals at a glance.

## Features Shown

- **Color-coded line numbers** with uniform black background boxes
- **Adaptive font system** for optimal text display
- **Real-time ETA** with color-coded urgency (white, yellow, red)
- **Dual ETA mode** showing next two departures per line (optional)
- **AC indicator** (asterisk) for air-conditioned vehicles
- **Czech character support** with proper diacritics (ř, ž, š, č, etc.)
- **Tabbed web configuration** with per-tab save
- **Custom line colors** with flexible pattern matching
- **Weather and rest mode** in status bar and web UI

---

[← Back to README](../README.md)
