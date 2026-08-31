#include "ConfigJson.h"

#include <ArduinoJson.h>
#include <string.h>

// Working document capacity for both directions.
//
// Sized from the MEASURED worst case, not estimated (2026-08-31, host probe):
// a typical Prague device exports 1,887 bytes; a Config with every string field
// filled to capacity exports 3,557. test_configjson's "maximally full config"
// case drives exactly that through both directions, so this number fails loudly
// if a field is ever widened rather than drifting out of date in a comment.
// Deliberately SMALLER than CONFIG_JSON_MAX_BYTES: a hostile file that fills
// the whole 8KB body with unknown keys then overflows this document and is
// rejected as ParseFailed, which leaves the caller's Config untouched. Failing
// closed on an oversized file is the intended behaviour, not a limitation.
#define CONFIG_JSON_DOC_BYTES 6144

// ---------------------------------------------------------------- small utils

// strlcpy is not portable to the desktop toolchain, and this file has to build
// there. Same contract: always terminates, never over-reads the source.
static void copyStr(char* dst, const char* src, size_t size)
{
    if (size == 0)
    {
        return;
    }
    size_t i = 0;
    for (; i + 1 < size && src[i] != '\0'; ++i)
    {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

static int clampInt(int value, int lo, int hi)
{
    return value < lo ? lo : (value > hi ? hi : value);
}

// ------------------------------------------------------------------ serialize

size_t configToJson(const Config& cfg, const char* board, const char* release, char* out, size_t outSize)
{
    DynamicJsonDocument doc(CONFIG_JSON_DOC_BYTES);

    // `cfg` is const, so every char array below decays to `const char*` and
    // ArduinoJson stores a POINTER rather than copying the string. That is what
    // keeps the export document small; taking a non-const Config& here would
    // silently switch it to copying and roughly double the allocation.
    doc["schema"] = CONFIG_JSON_SCHEMA;
    doc["release"] = release;
    doc["board"] = board;

    doc["wifiSsid"] = cfg.wifiSsid;
    doc["wifiPassword"] = cfg.wifiPassword;

    doc["city"] = cfg.city;
    doc["language"] = cfg.language;
    doc["pragueApiKey"] = cfg.pragueApiKey;
    doc["pragueStopIds"] = cfg.pragueStopIds;
    doc["berlinStopIds"] = cfg.berlinStopIds;

    doc["mqttBroker"] = cfg.mqttBroker;
    doc["mqttPort"] = cfg.mqttPort;
    doc["mqttUsername"] = cfg.mqttUsername;
    doc["mqttPassword"] = cfg.mqttPassword;
    doc["mqttRequestTopic"] = cfg.mqttRequestTopic;
    doc["mqttResponseTopic"] = cfg.mqttResponseTopic;
    doc["mqttUseEtaMode"] = cfg.mqttUseEtaMode;
    doc["mqttFieldLine"] = cfg.mqttFieldLine;
    doc["mqttFieldDestination"] = cfg.mqttFieldDestination;
    doc["mqttFieldEta"] = cfg.mqttFieldEta;
    doc["mqttFieldTimestamp"] = cfg.mqttFieldTimestamp;
    doc["mqttFieldPlatform"] = cfg.mqttFieldPlatform;
    doc["mqttFieldAC"] = cfg.mqttFieldAC;

    doc["refreshInterval"] = cfg.refreshInterval;
    doc["numDepartures"] = cfg.numDepartures;
    doc["minDepartureTime"] = cfg.minDepartureTime;
    doc["brightness"] = cfg.brightness;
    doc["lineColorMap"] = cfg.lineColorMap;
    doc["platformSymbolMap"] = cfg.platformSymbolMap;
    doc["debugMode"] = cfg.debugMode;
    doc["showPlatform"] = cfg.showPlatform;
    doc["scrollEnabled"] = cfg.scrollEnabled;
    doc["showMultipleTimes"] = cfg.showMultipleTimes;
    doc["restModePeriods"] = cfg.restModePeriods;

    doc["weatherEnabled"] = cfg.weatherEnabled;
    doc["weatherLatitude"] = cfg.weatherLatitude;
    doc["weatherLongitude"] = cfg.weatherLongitude;
    doc["weatherRefreshInterval"] = cfg.weatherRefreshInterval;

    doc["tickerEnabled"] = cfg.tickerEnabled;
    doc["tickerSymbol"] = cfg.tickerSymbol;
    doc["tickerInterval"] = cfg.tickerInterval;
    doc["tickerApiKey"] = cfg.tickerApiKey;
    doc["tickerRefreshInterval"] = cfg.tickerRefreshInterval;

    // Panel arrangement. `dispGeom` is authoritative; `panelRows` is written
    // alongside it for a human reading the file and for an older firmware that
    // predates dispGeom, NOT as a second source of truth -- the importer
    // prefers dispGeom whenever both are present.
    doc["dispGeom"] = (int)cfg.geometry;
    doc["panelRows"] = geometryPanelRows(cfg.geometry);

    // Wiring, by connector position. An object rather than an array so a
    // hand-edited file cannot silently transpose two signals by miscounting.
    doc["hwCustomPins"] = cfg.hwProfile.useCustomPins;
    doc["hwRgbOrder"] = (int)cfg.hwProfile.order;
    doc["hwDriver"] = cfg.hwProfile.driver;
    JsonObject pins = doc.createNestedObject("hwPins");
    pins["r1"] = cfg.hwProfile.pins.r1;
    pins["g1"] = cfg.hwProfile.pins.g1;
    pins["b1"] = cfg.hwProfile.pins.b1;
    pins["r2"] = cfg.hwProfile.pins.r2;
    pins["g2"] = cfg.hwProfile.pins.g2;
    pins["b2"] = cfg.hwProfile.pins.b2;
    pins["a"] = cfg.hwProfile.pins.a;
    pins["b"] = cfg.hwProfile.pins.b;
    pins["c"] = cfg.hwProfile.pins.c;
    pins["d"] = cfg.hwProfile.pins.d;
    pins["e"] = cfg.hwProfile.pins.e;
    pins["lat"] = cfg.hwProfile.pins.lat;
    pins["oe"] = cfg.hwProfile.pins.oe;
    pins["clk"] = cfg.hwProfile.pins.clk;

    if (doc.overflowed())
    {
        return 0;
    }

    const size_t written = serializeJsonPretty(doc, out, outSize);
    // serializeJson returns the bytes it COULD write; a truncated result is not
    // a valid document, so report failure rather than hand back half a config.
    if (written == 0 || written >= outSize)
    {
        return 0;
    }
    return written;
}

// -------------------------------------------------------------- apply helpers

static bool applyStr(JsonObjectConst o, const char* key, char* dst, size_t size, int& applied)
{
    if (!o.containsKey(key))
    {
        return false;
    }
    JsonVariantConst v = o[key];
    if (!v.is<const char*>())
    {
        return false;
    }
    const char* s = v.as<const char*>();
    if (s == nullptr)
    {
        return false;
    }
    copyStr(dst, s, size);
    applied++;
    return true;
}

static bool applyInt(JsonObjectConst o, const char* key, int& dst, int& applied)
{
    if (!o.containsKey(key))
    {
        return false;
    }
    JsonVariantConst v = o[key];
    if (!v.is<int>())
    {
        return false;
    }
    dst = v.as<int>();
    applied++;
    return true;
}

static bool applyBool(JsonObjectConst o, const char* key, bool& dst, int& applied)
{
    if (!o.containsKey(key))
    {
        return false;
    }
    JsonVariantConst v = o[key];
    if (!v.is<bool>())
    {
        return false;
    }
    dst = v.as<bool>();
    applied++;
    return true;
}

static bool applyFloat(JsonObjectConst o, const char* key, float& dst, int& applied)
{
    if (!o.containsKey(key))
    {
        return false;
    }
    JsonVariantConst v = o[key];
    if (!v.is<float>())
    {
        return false;
    }
    dst = v.as<float>();
    applied++;
    return true;
}

static bool applyPin(JsonObjectConst o, const char* key, int8_t& dst)
{
    if (!o.containsKey(key))
    {
        return false;
    }
    JsonVariantConst v = o[key];
    if (!v.is<int>())
    {
        return false;
    }
    dst = (int8_t)v.as<int>();
    return true;
}

// -------------------------------------------------------------------- import

ConfigImportResult configFromJson(const char* json, Config& cfg, const char* thisBoard,
                                  const ConfigImportOptions& options)
{
    ConfigImportResult result = {};
    result.status = ConfigImportStatus::Ok;
    result.fileBoard[0] = '\0';

    if (json == nullptr)
    {
        result.status = ConfigImportStatus::ParseFailed;
        return result;
    }

    DynamicJsonDocument doc(CONFIG_JSON_DOC_BYTES);
    const DeserializationError err = deserializeJson(doc, json);
    if (err)
    {
        // Covers malformed, truncated AND too-large-for-the-document. All three
        // must leave `cfg` byte-identical, which is why nothing above this point
        // has written to it.
        result.status = ConfigImportStatus::ParseFailed;
        return result;
    }
    if (!doc.is<JsonObject>())
    {
        result.status = ConfigImportStatus::NotAnObject;
        return result;
    }

    JsonObjectConst o = doc.as<JsonObjectConst>();

    // A document with no schema key is treated as schema 1 -- the first format
    // -- rather than rejected, so a hand-written minimal file works.
    const int schema = o.containsKey("schema") ? (int)(o["schema"] | CONFIG_JSON_SCHEMA) : CONFIG_JSON_SCHEMA;
    if (schema > CONFIG_JSON_SCHEMA)
    {
        result.status = ConfigImportStatus::SchemaTooNew;
        return result;
    }

    // `board` is IDENTITY. It is read for the comparison and for the message,
    // and is deliberately never copied into `cfg` -- there is no Config field
    // for it, and that absence is the enforcement.
    if (o.containsKey("board"))
    {
        JsonVariantConst b = o["board"];
        if (b.is<const char*>() && b.as<const char*>() != nullptr)
        {
            copyStr(result.fileBoard, b.as<const char*>(), sizeof(result.fileBoard));
        }
    }
    result.boardMismatch =
        (result.fileBoard[0] != '\0') && (thisBoard != nullptr) && (strcmp(result.fileBoard, thisBoard) != 0);

    int n = 0;

    applyStr(o, "wifiSsid", cfg.wifiSsid, sizeof(cfg.wifiSsid), n);
    applyStr(o, "wifiPassword", cfg.wifiPassword, sizeof(cfg.wifiPassword), n);

    applyStr(o, "city", cfg.city, sizeof(cfg.city), n);
    applyStr(o, "language", cfg.language, sizeof(cfg.language), n);
    applyStr(o, "pragueApiKey", cfg.pragueApiKey, sizeof(cfg.pragueApiKey), n);
    applyStr(o, "pragueStopIds", cfg.pragueStopIds, sizeof(cfg.pragueStopIds), n);
    applyStr(o, "berlinStopIds", cfg.berlinStopIds, sizeof(cfg.berlinStopIds), n);

    applyStr(o, "mqttBroker", cfg.mqttBroker, sizeof(cfg.mqttBroker), n);
    applyInt(o, "mqttPort", cfg.mqttPort, n);
    applyStr(o, "mqttUsername", cfg.mqttUsername, sizeof(cfg.mqttUsername), n);
    applyStr(o, "mqttPassword", cfg.mqttPassword, sizeof(cfg.mqttPassword), n);
    applyStr(o, "mqttRequestTopic", cfg.mqttRequestTopic, sizeof(cfg.mqttRequestTopic), n);
    applyStr(o, "mqttResponseTopic", cfg.mqttResponseTopic, sizeof(cfg.mqttResponseTopic), n);
    applyBool(o, "mqttUseEtaMode", cfg.mqttUseEtaMode, n);
    applyStr(o, "mqttFieldLine", cfg.mqttFieldLine, sizeof(cfg.mqttFieldLine), n);
    applyStr(o, "mqttFieldDestination", cfg.mqttFieldDestination, sizeof(cfg.mqttFieldDestination), n);
    applyStr(o, "mqttFieldEta", cfg.mqttFieldEta, sizeof(cfg.mqttFieldEta), n);
    applyStr(o, "mqttFieldTimestamp", cfg.mqttFieldTimestamp, sizeof(cfg.mqttFieldTimestamp), n);
    applyStr(o, "mqttFieldPlatform", cfg.mqttFieldPlatform, sizeof(cfg.mqttFieldPlatform), n);
    applyStr(o, "mqttFieldAC", cfg.mqttFieldAC, sizeof(cfg.mqttFieldAC), n);

    applyInt(o, "refreshInterval", cfg.refreshInterval, n);
    applyInt(o, "numDepartures", cfg.numDepartures, n);
    applyInt(o, "minDepartureTime", cfg.minDepartureTime, n);
    applyInt(o, "brightness", cfg.brightness, n);
    applyStr(o, "lineColorMap", cfg.lineColorMap, sizeof(cfg.lineColorMap), n);
    applyStr(o, "platformSymbolMap", cfg.platformSymbolMap, sizeof(cfg.platformSymbolMap), n);
    applyBool(o, "debugMode", cfg.debugMode, n);
    applyBool(o, "showPlatform", cfg.showPlatform, n);
    applyBool(o, "scrollEnabled", cfg.scrollEnabled, n);
    applyBool(o, "showMultipleTimes", cfg.showMultipleTimes, n);
    applyStr(o, "restModePeriods", cfg.restModePeriods, sizeof(cfg.restModePeriods), n);

    applyBool(o, "weatherEnabled", cfg.weatherEnabled, n);
    applyFloat(o, "weatherLatitude", cfg.weatherLatitude, n);
    applyFloat(o, "weatherLongitude", cfg.weatherLongitude, n);
    applyInt(o, "weatherRefreshInterval", cfg.weatherRefreshInterval, n);

    applyBool(o, "tickerEnabled", cfg.tickerEnabled, n);
    applyStr(o, "tickerSymbol", cfg.tickerSymbol, sizeof(cfg.tickerSymbol), n);
    applyStr(o, "tickerInterval", cfg.tickerInterval, sizeof(cfg.tickerInterval), n);
    applyStr(o, "tickerApiKey", cfg.tickerApiKey, sizeof(cfg.tickerApiKey), n);
    applyInt(o, "tickerRefreshInterval", cfg.tickerRefreshInterval, n);

    // -------- panel arrangement: opt-in, but portable across boards
    //
    // Geometry describes the PANELS, not the controller, so a board mismatch
    // does not veto it -- only the user's checkbox governs.
    if (options.restoreGeometry)
    {
        int geom = 0;
        if (applyInt(o, "dispGeom", geom, n))
        {
            cfg.geometry = (geom >= 1 && geom <= 3) ? (PanelGeometry)geom : PanelGeometry::Chain2x32;
            result.geometryApplied = true;
        }
        else
        {
            // D3: a file written before dispGeom existed carries only panelRows.
            // Run the same migration ladder loadConfig() runs, so "2" means the
            // 2x2 grid here exactly as it does at boot.
            int legacyRows = 0;
            if (applyInt(o, "panelRows", legacyRows, n))
            {
                cfg.geometry = geometryFromLegacyPanelRows(legacyRows);
                result.geometryApplied = true;
            }
        }
    }

    // -------- wiring: opt-in AND board-locked
    //
    // These are GPIO numbers on THIS controller. A pin map from another board
    // is refused even with the box ticked -- that is the one thing the board
    // stamp actually vetoes.
    if (options.restoreWiring)
    {
        if (result.boardMismatch)
        {
            result.wiringRefused = true;
        }
        else if (o.containsKey("hwPins"))
        {
            JsonVariantConst pv = o["hwPins"];
            if (pv.is<JsonObjectConst>())
            {
                JsonObjectConst p = pv.as<JsonObjectConst>();
                HubPins candidate = cfg.hwProfile.pins;
                applyPin(p, "r1", candidate.r1);
                applyPin(p, "g1", candidate.g1);
                applyPin(p, "b1", candidate.b1);
                applyPin(p, "r2", candidate.r2);
                applyPin(p, "g2", candidate.g2);
                applyPin(p, "b2", candidate.b2);
                applyPin(p, "a", candidate.a);
                applyPin(p, "b", candidate.b);
                applyPin(p, "c", candidate.c);
                applyPin(p, "d", candidate.d);
                applyPin(p, "e", candidate.e);
                applyPin(p, "lat", candidate.lat);
                applyPin(p, "oe", candidate.oe);
                applyPin(p, "clk", candidate.clk);

                // hwResolvePins() would drop an invalid map at boot anyway, but
                // storing one means the Hardware tab shows a wiring the device
                // is not using. Refuse it here instead.
                if (hwValidatePins(candidate) == HwPinError::None)
                {
                    cfg.hwProfile.pins = candidate;
                    int order = 0;
                    if (applyInt(o, "hwRgbOrder", order, n))
                    {
                        cfg.hwProfile.order = (order >= 0 && order <= (int)RgbOrder::BGR)
                                                  ? (RgbOrder)order
                                                  : hwDefaultRgbOrder(cfg.geometry);
                    }
                    int driver = 0;
                    if (applyInt(o, "hwDriver", driver, n))
                    {
                        cfg.hwProfile.driver = (uint8_t)clampInt(driver, 0, 5);
                    }
                    applyBool(o, "hwCustomPins", cfg.hwProfile.useCustomPins, n);
                    n++;
                    result.wiringApplied = true;
                }
            }
        }
    }

    // Same function loadConfig() calls, so an import cannot land a value a boot
    // would have rejected. This is the acceptance criterion made structural.
    configClamp(cfg);

    result.fieldsApplied = n;
    return result;
}

// --------------------------------------------------------------------- clamp

void configClamp(Config& cfg)
{
    // Geometry first: numDepartures is bounded BY it, so repairing an
    // out-of-range arrangement afterwards would clamp rows against a value
    // about to change.
    const int g = (int)cfg.geometry;
    if (g < 1 || g > 3)
    {
        cfg.geometry = PanelGeometry::Chain2x32;
    }
    cfg.panelRows = geometryPanelRows(cfg.geometry);

    cfg.refreshInterval = clampInt(cfg.refreshInterval, 10, 300);
    cfg.minDepartureTime = clampInt(cfg.minDepartureTime, 0, 30);
    cfg.brightness = clampInt(cfg.brightness, 0, 255);
    cfg.numDepartures = clampInt(cfg.numDepartures, 1, geometryMaxDepartureRows(cfg.geometry));
    // 5..120 is what the Optional tab's number input advertises (min=5 max=120)
    // and what every release has loaded. parseOptionalSettings() used to narrow
    // saves to 10..60, so the form offered a range it silently refused to keep;
    // that clamp is gone and this is the definition.
    cfg.weatherRefreshInterval = clampInt(cfg.weatherRefreshInterval, 5, 120);
    cfg.mqttPort = clampInt(cfg.mqttPort, 1, 65535);
    cfg.tickerRefreshInterval = clampInt(cfg.tickerRefreshInterval, 120, 600);

    if ((int)cfg.hwProfile.order < 0 || (int)cfg.hwProfile.order > (int)RgbOrder::BGR)
    {
        cfg.hwProfile.order = hwDefaultRgbOrder(cfg.geometry);
    }
    cfg.hwProfile.driver = (uint8_t)clampInt(cfg.hwProfile.driver, 0, 5);

    // An empty map is not a valid configuration -- loadConfig() substitutes the
    // Prague defaults rather than rendering every line in the fallback colour.
    if (cfg.lineColorMap[0] == '\0')
    {
        copyStr(cfg.lineColorMap, DEFAULT_LINE_COLOR_MAP, sizeof(cfg.lineColorMap));
    }
}
