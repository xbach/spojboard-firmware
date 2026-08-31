# Using Generic ESP32-S3 Boards

> **Note:** This document is for reference only. SpojBoard now includes native support for generic ESP32-S3 boards with **automatic pin mapping** - no code changes required!
>
> **→ See [Wiring Guide](WIRING.md) for the recommended setup with standard HUB75 cables.**

## Quick Start (New Method)

1. **Flash the ESP32-S3 N8R2 firmware**: `spojboard-esp32_s3_n8r2-r*.bin`
2. **Wire using standard HUB75 cable**: Follow the pinout in [WIRING.md](WIRING.md)
3. **Done!** The firmware auto-detects hardware variant and configures pins accordingly.

No need to modify `platformio.ini` or `AppConfig.h` - just use the pre-built firmware for your board.

---

## Legacy Information (Custom Pin Mapping)

The information below is for advanced users who want to create custom hardware variants with different pin mappings.

### Overview

The SpojBoard firmware can run on any ESP32-S3 board with sufficient GPIO pins and flash memory. The main differences are pin configuration and physical wiring - the software architecture remains the same.

### A different pin map no longer needs a custom build

**Wire it however you like and set the pins in the web interface.** The **Hardware** tab exposes all
14 HUB75 GPIOs, the RGB channel order and the panel driver chip; changes apply on reboot. The pin map
is validated before it is used — duplicates, GPIO 22-25 or above 48, and the SPI-flash pins 26-32 are
rejected, and a rejected map falls back to the built-in one rather than reaching the driver. If a
valid-but-wrong map leaves the panel dark, **Restore built-in wiring** (also on that tab, and
available in setup mode) puts it back without a USB cable.

The **panel arrangement** — 2× 64×32, 4× 64×32 in a 2×2 grid, or 2× 64×64 / one 128×64 module — is a
setting too, under Display.

### Creating a custom hardware variant (rarely needed now)

A separate build environment is only worth it when something other than wiring differs — flash size,
partition layout, USB mode. In that case:

1. Add a new environment in `platformio.ini` with `custom_hardware_variant`
2. Set the board's default pin map in `src/config/AppConfig.h` (one connector-order table, plus
   `hwDefaultRgbOrder()` if the board transposes channels like the MatrixPortal does)
3. Build with `pio run -e your_variant`

Those macros are only the **factory default** a device falls back to; the stored profile wins.

### Requirements

- ESP32-S3 development board with **at least 8MB flash** (for OTA updates)
- **13 available GPIO pins** for the HUB75 interface — **14 if you use 64-high panels**, which need the E address line
- USB-C cable for programming
- External 5V power supply (see main README for details)

### Recommended Boards

- **ESP32-S3-DevKitC-1** (8MB/16MB flash version) - Native support via `esp32_s3_n8r2` variant
- **LOLIN S3** (16MB flash)
- **ESP32-S3-WROOM** based boards
- Any ESP32-S3 board with ≥8MB flash and 13+ free GPIOs

### Pin Selection Guidelines

**Safe to use:**
- GPIO 1-18 (most are safe for general use)
- GPIO 21
- GPIO 33-48

**Avoid these pins:**
- GPIO 0 (Boot button - may cause issues)
- GPIO 19, 20 (USB D-/D+)
- GPIO 26-32 (SPI flash/PSRAM - board dependent)
- Check your board schematic for strapping pins

### Level Shifting Considerations

ESP32-S3 outputs 3.3V logic. Most HUB75 panels work fine with 3.3V signals, but some panels may require 5V logic levels.

**Test without level shifters first:**
- If display looks bright and stable → you're good
- If display is dim, flickering, or unstable → add level shifters

**If level shifters are needed:**
- Use 74HCT245 octal bus transceivers
- You'll need 2 chips (16 channels) for all 13 signals
- Connect 3.3V from ESP32-S3 to A side, 5V to B side
- Set direction control to A→B (ESP32 to HUB75)

### Troubleshooting

See [WIRING.md](WIRING.md#troubleshooting) for common issues and solutions.
