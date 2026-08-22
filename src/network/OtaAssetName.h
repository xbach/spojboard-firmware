#ifndef OTA_ASSET_NAME_H
#define OTA_ASSET_NAME_H

#include <stddef.h>

// ============================================================================
// GitHub release asset name grammar
//
// Deliberately free of Arduino, ArduinoJson and network headers so it compiles
// and is tested on the desktop (`pio test -e native`, test/test_otaassets).
// This is the one piece of OTA that decides whether a device flashes firmware
// built for different hardware, so it is worth testing exhaustively somewhere
// that costs nothing to run.
//
// Grammar:
//     spojboard-<board>[_<display>]-r<release>-<buildid>.bin
//
//   <board>    matrixportal_s3 | esp32_s3_n8r2      (contains underscores!)
//   <display>  OPTIONAL, must look like <digits>x<digits>, e.g. 2x32, 2x64
//   <buildid>  8 hex chars, possibly followed by "-dirty"
//
// Because board names contain underscores, "split on the last _" is ambiguous:
//   esp32_s3_n8r2        -> would wrongly yield board=esp32_s3, display=n8r2
// so a trailing segment is only treated as a display token when it matches the
// <digits>x<digits> shape. That keeps bare and suffixed names distinguishable
// without a table of known boards.
//
// The release marker is found by scanning RIGHT TO LEFT for "-r<digits>-".
// Left-to-right search for the first "-r" is what makes a display suffix
// beginning with "r" (say "-rgb") silently truncate the board name and let a
// device accept firmware for other hardware. Right-to-left removes that trap
// by construction rather than by naming discipline.
// ============================================================================

enum class OtaAssetMatch
{
    None,    // not ours, or for another board
    Bare,    // spojboard-<board>-r<n>-<id>.bin        (no display suffix)
    Display, // spojboard-<board>_<display>-r<n>-<id>.bin
};

struct OtaAssetInfo
{
    OtaAssetMatch match;
    char display[16]; // "" unless match == Display
    int release;      // parsed release number, -1 if not parseable
};

/**
 * Classify a GitHub asset filename against this device's board.
 *
 * @param filename     asset name, e.g. "spojboard-matrixportal_s3-r9-1a2b3c4d.bin"
 * @param boardVariant this device's board, e.g. "matrixportal_s3" (VARIANT_NAME)
 * @return match kind plus the display token and release number when present
 */
OtaAssetInfo otaClassifyAsset(const char* filename, const char* boardVariant);

#endif // OTA_ASSET_NAME_H
