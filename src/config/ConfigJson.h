#ifndef CONFIG_JSON_H
#define CONFIG_JSON_H

#include "AppConfig.h"

// Config <-> JSON mapping for backup, restore and cloning (TA-0307).
//
// Pure: no Arduino, no Preferences, no WebServer. `AppConfig.h` is kept free of
// <Preferences.h> precisely so this file builds on the desktop, which is where
// test/test_configjson exercises it.
//
// SECRETS ARE EXPORTED IN PLAINTEXT, by decision. wifiPassword, pragueApiKey,
// mqttPassword and tickerApiKey all appear as-is. That is what makes an export
// a true clone-to-another-unit backup; the cost is that the file must be
// treated as a secret. The UI says so at both ends. There is deliberately no
// redaction mode -- a half-redacted backup that silently fails to restore a
// key is worse than one the user knows to guard.

// Bump only for a BREAKING change. Unknown keys are ignored, so adding a field
// does not need a new schema: an old device reading a new file simply keeps its
// own value for what it does not recognise.
#define CONFIG_JSON_SCHEMA 1

// Largest import body accepted. A real export is ~3-4KB; this is generous
// headroom that still bounds the JSON document allocation. Enforced by the
// caller before parsing, because the body is malloc'd by WebServer during
// request parsing -- before any handler runs -- so this cannot prevent the
// allocation, only the far larger one that parsing it would need.
#define CONFIG_JSON_MAX_BYTES 8192

enum class ConfigImportStatus : uint8_t
{
    Ok = 0,
    ParseFailed,   // not valid JSON, or truncated
    NotAnObject,   // valid JSON, but not a config document
    SchemaTooNew   // written by a firmware that changed the format incompatibly
};

// What an import may touch beyond the ordinary settings.
//
// Both default to FALSE: a restore must not be able to scramble a working panel
// by surprise. The split is by what the field actually DESCRIBES, which is why
// they are two flags and not one:
//   - geometry describes the PANELS (2x32 / 4x32 / 2x64). Portable to any board.
//   - wiring describes the GPIOs of THIS controller. Not portable across boards,
//     and refused outright on a board mismatch regardless of this flag.
struct ConfigImportOptions
{
    bool restoreGeometry;
    bool restoreWiring;
};

struct ConfigImportResult
{
    ConfigImportStatus status;
    bool boardMismatch;    // file's board stamp != this board
    bool wiringRefused;    // restoreWiring was asked for, but the board mismatched
    bool geometryApplied;
    bool wiringApplied;
    int fieldsApplied;     // count of recognised keys actually written
    char fileBoard[24];    // the board stamp found in the file ("" if absent)
};

// Serialize `cfg` as a JSON object into `out`. Returns the number of bytes
// written excluding the terminator, or 0 if `out` was too small.
//
// `board` and `release` are stamped into the document. `board` is IDENTITY: it
// is never read back into a Config by configFromJson, only compared.
size_t configToJson(const Config& cfg, const char* board, const char* release, char* out, size_t outSize);

// Parse `json` and apply it onto `cfg`.
//
// `cfg` MUST arrive holding the device's CURRENT configuration, not a blank
// struct: every field the document does not carry keeps the value already
// there. That single rule is what makes a partial or older export a safe
// restore, and it is why there is no "defaults" variant of this function.
//
// Applies nothing at all unless the whole document parses -- a truncated file
// leaves `cfg` byte-identical, so a caller that only persists on Ok can never
// half-write NVS.
ConfigImportResult configFromJson(const char* json, Config& cfg, const char* thisBoard,
                                  const ConfigImportOptions& options);

// Force every field into the range loadConfig() enforces at boot.
//
// Idempotent, and called from BOTH paths -- the importer and loadConfig -- so
// the guarantee "no import can produce a config a boot-time load would have
// rejected" holds by construction rather than by two lists agreeing.
void configClamp(Config& cfg);

#endif // CONFIG_JSON_H
