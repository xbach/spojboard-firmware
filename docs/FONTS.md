# SpojBoard Font System

Complete guide to character support and custom font creation for Central European languages.

## Table of Contents

- [Character Support](#character-support)
- [Creating Custom Fonts](#creating-custom-fonts)
- [Integration Guide](#integration-guide)
- [Available Fonts](#available-fonts)

## Character Support

SpojBoard implements full support for Central European diacritical characters using a custom 8-bit font system.

### The Problem

Standard Adafruit GFX fonts use 7-bit encoding (ASCII 0x20-0x7E), which doesn't include special characters like:
- **Czech**: ž, š, č, ř, ň, ť, ď, ú, ů, á, é, í, ó, ý
- **German**: ß, ẞ, ä, ö, ü, Ä, Ö, Ü
- **Polish/Hungarian**: ł, ń, ś, ź, ő, ű

Transit APIs (Golemio for Prague, BVG for Berlin) return station names in UTF-8 encoding (e.g., "Nádraží Hostivař", "Berliner Straße").

### The Solution

We use **8-bit ISO-8859-2 fonts** with automatic UTF-8 conversion:

#### 1. Custom Fonts
Generated from TrueType fonts using `fontconvert8` tool:
- Character range: 0x20-0xDF (192 printable characters)
- Includes full Latin Extended-A for Central European languages
- Stored as Adafruit GFX format in PROGMEM

#### 2. UTF-8 Decoder
Converts multi-byte UTF-8 to Unicode code points (RFC 3629):
- Located in [src/utils/decodeutf8.cpp/h](../src/utils/decodeutf8.h)
- Handles 1-4 byte UTF-8 sequences
- Returns Unicode code points (U+0000 to U+10FFFF)

#### 3. ISO-8859-2 Mapper
Maps Unicode to ISO-8859-2 with GFX encoding:
- Characters 0xA0-0xFF are shifted by -32 to fit in 0x80-0xDF range
- Located in [src/utils/gfxlatin2.cpp/h](../src/utils/gfxlatin2.h)
- In-place conversion using `utf8tocp(char* str)` function

### Conversion Examples

**Czech (Prague):**
```
UTF-8: "Nádraží"    → Bytes: 0x4E 0xC3 0xA1 0x64 0x72 0x61 0xC5 0xBE 0xC3 0xAD
                     ↓ decode UTF-8
Unicode: N á d r a ž í → Code points: U+004E U+00E1 U+0064 U+0072 U+0061 U+017E U+00ED
                     ↓ map to ISO-8859-2 with shift
ISO-8859-2: 0x4E 0xC1 0x64 0x72 0x61 0xBE 0xCD → Display correctly on LED matrix
```

**German (Berlin):**
```
UTF-8: "Straße"     → Bytes: 0x53 0x74 0x72 0x61 0xC3 0x9F 0x65
                     ↓ decode UTF-8
Unicode: S t r a ß e → Code points: U+0053 U+0074 U+0072 U+0061 U+00DF U+0065
                     ↓ map to ISO-8859-2 with shift
ISO-8859-2: 0x53 0x74 0x72 0x61 0xCF 0x65 → Display correctly on LED matrix
```

### Usage in Code

```cpp
#include "../fonts/DepartureMono5pt8b.h"
#include "../utils/gfxlatin2.h"

const GFXfont* fontMedium = &DepartureMono_Regular5pt8b;

// Get UTF-8 string from API
char destination[32];
strlcpy(destination, "Nádraží Hostivař", sizeof(destination));

// Convert to ISO-8859-2 (in-place)
utf8tocp(destination);

// Display with proper Czech characters
display->setFont(fontMedium);
display->setTextColor(COLOR_WHITE);
display->setCursor(x, y);
display->print(destination);  // Correctly shows "ř" and other diacritics
```

### Font API (Adafruit GFX)

- `display->setFont(const GFXfont*)` - Switch font
- `display->setTextColor(uint16_t)` - Set foreground color (transparent background)
- `display->setCursor(int16_t x, int16_t y)` - Position cursor
- `display->getTextBounds(const char*, int16_t, int16_t, int16_t*, int16_t*, uint16_t*, uint16_t*)` - Measure text dimensions
- `display->print(const char*)` - Render text

All fonts are stored in PROGMEM to save RAM.

### Credits

Based on work by:
- [Michel Deslierres](https://sigmdel.ca/michel/program/misc/gfxfont_8bit_en.html) - Original UTF-8 to ISO-8859-2 conversion code
- [Petr Brouzda - fontconvert8-iso8859-2](https://github.com/petrbrouzda/fontconvert8-iso8859-2) - Font conversion tool and UTF-8 conversion implementation

## Creating Custom Fonts

To create your own 8-bit ISO-8859-2 fonts:

### Prerequisites

- TrueType (.ttf) or OpenType (.otf) font file
- Modified fontconvert8 tool (works on Mac M-series)

### Using fontconvert8-iso8859-2

The [fontconvert8-iso8859-2](https://github.com/petrbrouzda/fontconvert8-iso8859-2) repository contains the necessary tools with modifications for macOS compatibility.

#### Installation

```bash
# Clone the tool repository
git clone https://github.com/petrbrouzda/fontconvert8-iso8859-2.git
cd fontconvert8-iso8859-2/fontconvert8

# Build the tool (may need modifications for M-series Macs)
make
```

#### Basic Usage

```bash
# Convert a font
./fontconvert YourFont.ttf 12 > YourFont12pt8b.h

# Parameters:
# - YourFont.ttf: Input font file
# - 12: Point size
# - > YourFont12pt8b.h: Output header file
```

**Font naming convention**: `FontName[size]pt8b.h`
- `8b` = 8-bit encoding
- Size = point size (e.g., 5pt, 12pt)

### Mac M-Series Compatibility

The original fontconvert8 tool may require modifications to compile on Apple Silicon. The fork used in this project includes necessary adjustments for ARM64 architecture.

If you encounter build issues:

1. **Install FreeType2 via Homebrew**:
   ```bash
   brew install freetype
   ```

2. **Update Makefile compiler flags**:
   ```makefile
   CFLAGS += -I/opt/homebrew/include/freetype2
   LDFLAGS += -L/opt/homebrew/lib -lfreetype
   ```

3. **Adjust include paths for ARM architecture**:
   - Check FreeType2 installation path: `brew --prefix freetype`
   - Update Makefile accordingly

## Integration Guide

### Step 1: Generate Font File

```bash
cd fontconvert8-iso8859-2/fontconvert8
./fontconvert YourFont.ttf 5 > DepartureMonoCustom5pt8b.h
```

### Step 2: Copy to Project

```bash
cp DepartureMonoCustom5pt8b.h /path/to/spojboard-firmware/src/fonts/
```

### Step 3: Include in Code

Edit [src/display/DisplayManager.cpp](../src/display/DisplayManager.cpp):

```cpp
// Add include at top
#include "../fonts/DepartureMonoCustom5pt8b.h"

// In DisplayManager constructor
DisplayManager::DisplayManager()
    : display(nullptr), isDrawing(false), config(nullptr)
{
    fontSmall = &DepartureMono_Regular4pt8b;
    fontMedium = &DepartureMonoCustom5pt8b;  // Your custom font
    fontCondensed = &DepartureMono_Condensed5pt8b;
}
```

### Step 4: Rebuild and Flash

```bash
pio run -t upload
```

## Fine-Tuning with GFX Font Customiser

After generating fonts with fontconvert8, you can fine-tune individual characters using the [Adafruit GFX Font Customiser by tchapi](https://tchapi.github.io/Adafruit-GFX-Font-Customiser/).

### Features

- **Visualize** the generated font in a browser
- **Edit individual glyphs** pixel by pixel
- **Adjust spacing** and kerning
- **Add or modify characters** not in the original font
- **Export** the modified font as a .h file

### Workflow

1. **Generate base font** with fontconvert8
2. **Open the .h file** in the GFX Font Customiser web tool
3. **Make visual adjustments** to improve readability on LED matrix:
   - Adjust character width/height
   - Fix pixel alignment issues
   - Optimize spacing between characters
   - Ensure diacritics don't overlap
4. **Export** and replace the original .h file
5. **Rebuild** firmware and test on device

### Tips for LED Matrix Optimization

- **High contrast**: Use solid shapes, avoid thin lines (may not be visible)
- **Even spacing**: Maintain consistent character spacing for readability
- **Diacritics**: Ensure accents don't touch base characters
- **Test on device**: What looks good on screen may differ on LED matrix

## Available Fonts

SpojBoard includes three custom fonts optimized for LED matrix display:

### DepartureMono_Regular4pt8b
- **Size**: 4pt (small)
- **Use case**: Compact text, line numbers, status messages
- **Character width**: ~3-4 pixels
- **Line height**: 6 pixels
- **File**: [src/fonts/DepartureMono4pt8b.h](../src/fonts/DepartureMono4pt8b.h)

### DepartureMono_Regular5pt8b
- **Size**: 5pt (medium)
- **Use case**: Destinations, larger text, ETAs
- **Character width**: ~4-5 pixels
- **Line height**: 8 pixels
- **File**: [src/fonts/DepartureMono5pt8b.h](../src/fonts/DepartureMono5pt8b.h)

### DepartureMono_Condensed5pt8b
- **Size**: 5pt condensed
- **Use case**: Long destinations, secondary ETAs, absolute departure times (>60min)
- **Character width**: ~3-4 pixels (narrower than regular)
- **Line height**: 8 pixels
- **File**: [src/fonts/DepartureMonoCondensed5pt8b.h](../src/fonts/DepartureMonoCondensed5pt8b.h)

### Font Selection Logic

The display automatically chooses the appropriate font based on destination length and available space. Font selection is handled by `calcDestLayout()` in `DisplayManager.cpp` using a threshold-based system rather than any fixed character constant:

- A `mediumThreshold` is computed from the active display features:
  - **14** chars when only the primary ETA is shown
  - **12** chars when platform symbols **or** dual ETA **or** absolute departure time is active
  - **11** chars when both platform symbols **and** dual ETA are active
  - minus **1** if the AC indicator is present on the row
- Destinations at or below the threshold use the regular (medium) font; longer destinations switch to the condensed font
- `maxChars` (the truncation/scroll limit) is computed from the actual available pixel width after subtracting the route box, ETA area, platform reservation, and AC indicator — capped at 63
- Horizontal scrolling triggers when the destination exceeds the available pixel width (there is no fixed "16 char" or "23 char" cutoff)

## Character Set Coverage

All fonts include the full ISO-8859-2 character set:

### Basic Latin (ASCII)
- **Range**: U+0020 to U+007E
- **Characters**: A-Z, a-z, 0-9, punctuation

### Latin Extended-A (Czech diacritics)
- **Uppercase**: Á É Í Ó Ú Ý Č Ď Ě Ň Ř Š Ť Ů Ž
- **Lowercase**: á é í ó ú ý č ď ě ň ř š ť ů ž

### Additional Central European Characters
- **Polish**: Ą Ć Ę Ł Ń Ó Ś Ź Ż (ą ć ę ł ń ó ś ź ż)
- **Hungarian**: Ő Ű (ő ű)
- **Slovak**: Ľ Ĺ Ŕ (ľ ĺ ŕ)
- **German**: Ä Ö Ü ß (ä ö ü)

### Special Characters
- Currency: € (Euro) - mapped from special position
- Symbols: ° § ± × ÷
- Punctuation: « » „ "

## Testing Fonts

### On Device

1. **Flash firmware** with new font
2. **Configure stop** with Czech characters in destination
3. **Observe display** for:
   - Correct diacritics rendering
   - Proper spacing
   - No character overlap
   - Readability at viewing distance

### Test Strings

Use these test strings to verify character coverage:

```cpp
// Czech pangram (uses all diacritics)
"Příliš žluťoučký kůň úpěl ďábelské ódy"

// Common station names
"Nádraží Hostivař"
"Karlovo náměstí"
"Můstek"
"Muzeum"
"Florenc"
"Černý Most"

// Numbers and punctuation
"Line 1-99: Depo → Sídliště"
```

## Troubleshooting

### Missing Characters
**Symptom**: Square boxes or gaps instead of characters
**Cause**: Font doesn't include the character
**Solution**: Regenerate font with fontconvert8 (includes full ISO-8859-2 by default)

### Garbled Text
**Symptom**: Wrong characters displayed
**Cause**: Forgot to call `utf8tocp()` before display
**Solution**: Always convert UTF-8 strings before rendering:
```cpp
utf8tocp(destination);  // Must call before display->print()
```

### Overlapping Characters
**Symptom**: Characters touch or overlap
**Cause**: Incorrect spacing in font file
**Solution**: Use GFX Font Customiser to adjust character spacing

### Diacritics Cut Off
**Symptom**: Top or bottom of accented characters missing
**Cause**: Insufficient line height or wrong cursor position
**Solution**:
- Check cursor Y position (should align with baseline)
- Verify line height in font definition
- Test with `getTextBounds()` to verify bounding box

## Performance Considerations

### Memory Usage
- Each font: ~2-4 KB in PROGMEM (flash storage)
- No RAM overhead (fonts in flash, not loaded to RAM)
- Character lookups: O(1) by array index

### Rendering Speed
- Character rendering: ~0.1ms per character
- String rendering: ~1-2ms for typical destination (15 chars)
- UTF-8 conversion: <0.1ms (one-time, before caching)

### Optimization Tips
- **Convert once**: Call `utf8tocp()` when receiving API data, not before every display update
- **Cache results**: Store converted strings in departure cache
- **Minimize redraws**: Only update display when data changes
