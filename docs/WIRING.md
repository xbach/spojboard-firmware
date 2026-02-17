# HUB75 Wiring Guide

This document explains the correct physical wiring for different hardware variants.

## Automatic Pin Mapping

✅ **Good news:** The firmware automatically adjusts pin mappings based on your hardware variant. You can use **standard HUB75 cables without any modifications** - just plug and play!

The firmware detects whether you're using a MatrixPortal S3 or generic ESP32-S3 and configures the pins accordingly.

## MatrixPortal ESP32-S3

**Connection:** Direct HUB75 connector on board

The MatrixPortal has a built-in HUB75 connector. Simply plug your panel's cable directly into the board - no wiring required.

**Pin mapping (internal to MatrixPortal):**
```
HUB75 Connector → ESP32-S3 GPIO
─────────────────────────────────
R1  → GPIO 42
G1  → GPIO 40
B1  → GPIO 41
GND → GND
R2  → GPIO 38
G2  → GPIO 37
B2  → GPIO 39
E   → GPIO 21
A   → GPIO 45
B   → GPIO 36
C   → GPIO 48
D   → GPIO 35
CLK → GPIO 2
LAT → GPIO 47
OE  → GPIO 14
GND → GND
```

## ESP32-S3 N8R2 DevKit (Generic)

**Connection:** HUB75 cable with breakout to dupont pins

> **3.3V Logic Warning:** The ESP32-S3 outputs 3.3V signals, but HUB75 panels expect 5V logic. This works with many panels but is out of spec and may cause issues with some panel batches. For guaranteed reliability, add a 74AHCT245 level shifter between the ESP32-S3 and the panel's data/control lines. The MatrixPortal S3 (Option 1) includes these level shifters on-board.

✅ **Just use standard HUB75 wiring** - the firmware automatically handles the pin mapping differences!

### Rainbow Cable Wiring (Standard HUB75 Pinout)

```
Cable Color   HUB75 Label   →   ESP32-S3 GPIO
────────────────────────────────────────────────
1.  Brown      R1           →   GPIO 42
2.  Orange     G1           →   GPIO 41
3.  Yellow     B1           →   GPIO 40
4.  Green      GND          →   GND
5.  Blue       R2           →   GPIO 38
6.  Purple     G2           →   GPIO 39
7.  Gray       B2           →   GPIO 37
8.  White      E            →   GPIO 21
9.  Black      A            →   GPIO 45
10. Red        B            →   GPIO 36
11. Orange     C            →   GPIO 48
12. Yellow     D            →   GPIO 35
13. Green      CLK          →   GPIO 2
14. Blue       LAT          →   GPIO 47
15. Purple     OE           →   GPIO 14
16. White      GND          →   GND
```

### How It Works

The firmware detects that you're running the `esp32_s3_n8r2` variant and automatically maps:
- `G1_PIN` → GPIO 41 (where standard HUB75 green wire connects)
- `B1_PIN` → GPIO 40 (where standard HUB75 blue wire connects)
- `G2_PIN` → GPIO 39 (where standard HUB75 green wire connects)
- `B2_PIN` → GPIO 37 (where standard HUB75 blue wire connects)

This compensates for the MatrixPortal's non-standard connector layout, so you can use any standard HUB75 cable or tutorial.

## Standard HUB75 Pinout Reference

For reference, the standard HUB75 2×8 connector pinout is:

```
Top Row (Pins 1-8):
┌────┬────┬────┬────┬────┬────┬────┬────┐
│ R1 │ G1 │ B1 │GND │ R2 │ G2 │ B2 │ E  │
└────┴────┴────┴────┴────┴────┴────┴────┘

Bottom Row (Pins 9-16):
┌────┬────┬────┬────┬────┬────┬────┬────┐
│ A  │ B  │ C  │ D  │CLK │LAT │ OE │GND │
└────┴────┴────┴────┴────┴────┴────┴────┘
```

## Power Connections

**Measured power requirements:** SpojBoard draws 0.3-0.7A during normal operation, with transient peaks up to 1.6A during WiFi connection, OTA updates, or boot sequences.

### Connection Options

**MatrixPortal S3:**
- Power via MatrixPortal's **USB-C port** (5V 2A+ adapter) **and** connect display power via **screw terminals** (5V/GND)
- The MatrixPortal passes USB-C power through to the LED panels via screw terminals
- Use any quality USB-C phone charger (2A or higher)

**ESP32-S3 N8R2:**
- Power LED panels directly via their screw terminals from separate 5V supply (2A+ minimum, 3A recommended)
- Power ESP32 board via USB for programming/serial (can remain connected)
- **Important**: Connect ESP32 GND to panel GND for common ground reference

### Power Supply Recommendations

| Supply Type | Rating | Suitability | Notes |
|-------------|--------|-------------|-------|
| **USB-C Wall Adapter** | 5V 2A (10W) | ✅ Adequate | Covers all normal operation + transient peaks |
| **USB-C Wall Adapter** | 5V 3A (15W) | ✅ Recommended | Extra safety margin for simultaneous loads |
| **USB-C Wall Adapter** | 5V 1A (5W) | ❌ Insufficient | Will cause brownouts during WiFi activity |
| **Screw Terminal PSU** | 5V 2-3A | ✅ For ESP32-S3 N8R2 | Use with generic dev boards |

### Compatible Power Supplies

**USB-C (recommended for MatrixPortal S3):**
- Apple 12W USB-C adapter (2.4A)
- Anker PowerPort III Nano (2A+)
- Samsung EP-TA20 (2A)
- Any quality USB-C phone charger rated 2A or higher

**Screw Terminal Supplies (for ESP32-S3 N8R2):**
- Mean Well RS-15-5 (5V 3A)
- Any regulated 5V 2-3A DC power supply with screw terminal output

⚠️ **Never power the panels from the ESP32's 5V pin** - it cannot supply enough current and will cause the board to reset.

### Understanding Power Specifications

Published HUB75 panel specifications often cite 4-6A current draw at full brightness with all LEDs displaying white. However, SpojBoard's real-world usage (text display, mixed colors, brightness 90 default) draws significantly less power:

- **Theoretical maximum** (all white, brightness 255): ~4-6A per manufacturer specs
- **Measured actual usage** (text display, brightness 90): ~0.3A average
- **Measured worst case** (text display, brightness 255): ~0.7A peak

The difference is due to:
1. Text displays use far fewer LEDs than solid colors
2. Mixed colors (red/green/blue) draw less current than white
3. Default brightness (90/255 = 35%) significantly reduces power
4. Most pixels are black (off) in typical departure display

A 2A supply provides 2.8× safety margin over measured peaks, while a 3A supply provides 4.3× margin.

## Troubleshooting

**Colors are wrong on generic ESP32-S3:**
- Verify you've swapped green/blue wires as documented above
- Check that all wires are firmly connected
- Ensure common ground between ESP32 and panel power supply

**Panel doesn't light up:**
- Check panel has separate 5V power supply connected
- Verify GND connection between ESP32 and panel
- Check all 16 signal wires are connected

**Flickering or artifacts:**
- Ensure common ground between ESP32 and panel
- Check for loose connections
- Verify power supply can provide sufficient current (4A+ recommended)
- If using generic ESP32-S3, consider adding a 74AHCT245 level shifter — marginal 3.3V signals can cause instability with some panels
