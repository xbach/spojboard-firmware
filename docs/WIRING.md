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

**Important:** LED panels draw significant current (2-4A per panel at full brightness).

- **MatrixPortal S3:** Power the panel using the MatrixPortal's screw terminals (5V/GND). The board passes power through to the panel.
- **ESP32-S3 N8R2:** Power the panel directly from a separate 5V power supply (4A+). Connect ESP32 GND to panel GND for common ground reference.

⚠️ Never power the panel from the ESP32's 5V pin - it cannot supply enough current.

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
