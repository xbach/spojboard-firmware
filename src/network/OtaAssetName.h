#ifndef OTA_ASSET_NAME_H
#define OTA_ASSET_NAME_H

#include <stddef.h>

// ============================================================================
// GitHub release asset name grammar
//
// Deliberately free of Arduino, ArduinoJson and network headers so it compiles
// and is tested on the desktop (`pio test -e native`, test/test_otaassets).
// This is the code that decides whether a device flashes an image built for
// different hardware, so it is worth testing exhaustively where runs are free.
//
//     spojboard-<board>-[<display>-]r<release>-<buildid>[-dirty].bin
//
//   <board>    matrixportal_s3 | esp32_s3_n8r2
//   <display>  OPTIONAL geometry token, e.g. 2x32, 4x32, 2x64
//   <buildid>  8 hex chars of the git SHA
//
// FIELDS ARE SEPARATED BY DASHES and parsed by splitting on them. Board names
// contain underscores (esp32_s3_n8r2) but never dashes, so every field boundary
// is unambiguous and no field needs to be recognised by its shape to find the
// others. An earlier version packed display into the board field as
// <board>_<display> and had to guess where the board ended, which only worked
// as long as no display token could be confused for part of a board name.
//
// It also disarms the "-r" trap by construction. r8's parser takes the text up
// to the FIRST "-r", so any field beginning with 'r' truncates the board name
// and makes r8 accept another board's firmware. Here the release field is the
// one matching exactly r<digits>, found by position among the split fields, so
// a value that merely starts with 'r' is never mistaken for it.
// ============================================================================

enum class OtaAssetMatch
{
    None,    // malformed, not ours, or for another board
    Bare,    // spojboard-<board>-r<n>-<id>.bin            (no display field)
    Display, // spojboard-<board>-<display>-r<n>-<id>.bin
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
 * @param filename     asset name, e.g. "spojboard-matrixportal_s3-2x32-r10-1a2b3c4d.bin"
 * @param boardVariant this device's board, e.g. "matrixportal_s3" (VARIANT_NAME)
 */
OtaAssetInfo otaClassifyAsset(const char* filename, const char* boardVariant);

#endif // OTA_ASSET_NAME_H
