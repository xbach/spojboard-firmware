#ifndef OTA_ASSET_SELECT_H
#define OTA_ASSET_SELECT_H

#include <ArduinoJson.h>
#include <stddef.h>

// Which release assets this device is offered, and in what order.
//
// Extracted from GitHubOTA so the POLICY is testable off-device: until then only
// otaClassifyAsset (the ingredient) had tests, while the loop that decides what
// a user actually sees ran only on hardware. ArduinoJson is header-only and
// builds natively, so this stays free of Arduino and network headers.
struct OtaAssetOption
{
    char name[64];    // Asset filename
    char url[256];    // Download URL
    char display[16]; // Geometry token, "" for a bare (geometry-agnostic) build
    size_t size;      // File size in bytes
};

// Collect every asset belonging to `board`, most specific first.
//
//   pass 0: spojboard-<board>-<display>-r<n>-<id>.bin   geometry-specific
//   pass 1: spojboard-<board>-r<n>-<id>.bin             bare
//
// Since panel arrangement became a runtime setting (TA-0303) releases publish
// one bare asset per board, so this normally returns 1 and the UI shows no
// chooser. The two-pass shape is kept deliberately: a build that genuinely
// cannot be expressed at runtime -- a panel needing a compiled-in scan-map
// remap is the known candidate -- would ship as a second asset and slot in
// ahead of the bare one with no code change.
//
// Assets for another board are never returned. Board mismatch is the one hard
// rejection: a wrong-board image means a different pin map and a dead display.
int otaCollectAssetOptions(JsonDocument& doc, const char* board, OtaAssetOption* out, int maxOptions);

#endif // OTA_ASSET_SELECT_H
