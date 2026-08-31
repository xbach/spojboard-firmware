// Config <-> JSON backup/restore (TA-0307).
//
// The contract these tests pin down, in one sentence each:
//   - an export carries EVERYTHING, secrets in plaintext, so it can clone a unit
//   - an import applies onto the CURRENT config, so an absent key keeps its value
//   - the two physical-display groups are opt-in, and split by what they describe
//   - a file that does not parse changes nothing at all
#include <unity.h>
#include <string.h>

#include "../../src/config/PanelGeometry.h"
#include "../../src/config/PanelGeometry.cpp"
#include "../../src/config/HardwareProfile.h"
#include "../../src/config/HardwareProfile.cpp"
#include "../../src/config/ConfigJson.h"
#include "../../src/config/ConfigJson.cpp"

static const char* THIS_BOARD = "matrixportal_s3";
static const char* OTHER_BOARD = "esp32_s3_n8r2";

// A config with every field set to something distinctive, so a field that fails
// to round-trip shows up as a mismatch rather than coincidentally matching a
// default.
static Config makeConfig()
{
    Config c = {};
    strcpy(c.wifiSsid, "HomeNet");
    strcpy(c.wifiPassword, "s3cr3t-wifi-pw");
    strcpy(c.pragueApiKey, "eyJhbGciOiJIUzI1NiJ9.some.jwt-with-an-email");
    strcpy(c.pragueStopIds, "U693Z2P,U321Z2P");
    strcpy(c.berlinStopIds, "900013102");
    strcpy(c.mqttBroker, "192.168.1.50");
    c.mqttPort = 1884;
    strcpy(c.mqttUsername, "spoj");
    strcpy(c.mqttPassword, "mqtt-secret");
    strcpy(c.mqttRequestTopic, "spoj/req");
    strcpy(c.mqttResponseTopic, "spoj/resp");
    c.mqttUseEtaMode = true;
    strcpy(c.mqttFieldLine, "ln");
    strcpy(c.mqttFieldDestination, "ds");
    strcpy(c.mqttFieldEta, "et");
    strcpy(c.mqttFieldTimestamp, "ts");
    strcpy(c.mqttFieldPlatform, "pf");
    strcpy(c.mqttFieldAC, "ac");
    c.refreshInterval = 45;
    c.numDepartures = 3;
    c.minDepartureTime = 4;
    c.brightness = 123;
    c.geometry = PanelGeometry::Chain2x32;
    c.panelRows = geometryPanelRows(c.geometry);
    strcpy(c.lineColorMap, "A=GREEN,9*?=CYAN");
    strcpy(c.platformSymbolMap, "B=3,ID:U693Z2P=7");
    strcpy(c.city, "Prague");
    strcpy(c.language, "cs");
    c.debugMode = true;
    c.showPlatform = true;
    c.scrollEnabled = true;
    c.showMultipleTimes = true;
    strcpy(c.restModePeriods, "23:00-06:00,13:00-14:00");
    c.weatherEnabled = true;
    c.weatherLatitude = 50.0755f;
    c.weatherLongitude = 14.4378f;
    c.weatherRefreshInterval = 20;
    c.tickerEnabled = true;
    strcpy(c.tickerSymbol, "BTC/USD");
    strcpy(c.tickerInterval, "4h");
    strcpy(c.tickerApiKey, "twelve-data-key");
    c.tickerRefreshInterval = 300;
    c.hwProfile.useCustomPins = true;
    c.hwProfile.pins = hwCompiledDefaultPins();
    c.hwProfile.pins.r1 = 10; // distinctive, so "wiring applied" is unambiguous
    c.hwProfile.order = RgbOrder::GBR;
    c.hwProfile.driver = 3;
    c.configured = true;
    return c;
}

static void assertSameSettings(const Config& a, const Config& b)
{
    TEST_ASSERT_EQUAL_STRING(a.wifiSsid, b.wifiSsid);
    TEST_ASSERT_EQUAL_STRING(a.wifiPassword, b.wifiPassword);
    TEST_ASSERT_EQUAL_STRING(a.pragueApiKey, b.pragueApiKey);
    TEST_ASSERT_EQUAL_STRING(a.pragueStopIds, b.pragueStopIds);
    TEST_ASSERT_EQUAL_STRING(a.berlinStopIds, b.berlinStopIds);
    TEST_ASSERT_EQUAL_STRING(a.mqttBroker, b.mqttBroker);
    TEST_ASSERT_EQUAL_INT(a.mqttPort, b.mqttPort);
    TEST_ASSERT_EQUAL_STRING(a.mqttUsername, b.mqttUsername);
    TEST_ASSERT_EQUAL_STRING(a.mqttPassword, b.mqttPassword);
    TEST_ASSERT_EQUAL_STRING(a.mqttRequestTopic, b.mqttRequestTopic);
    TEST_ASSERT_EQUAL_STRING(a.mqttResponseTopic, b.mqttResponseTopic);
    TEST_ASSERT_EQUAL(a.mqttUseEtaMode, b.mqttUseEtaMode);
    TEST_ASSERT_EQUAL_STRING(a.mqttFieldLine, b.mqttFieldLine);
    TEST_ASSERT_EQUAL_STRING(a.mqttFieldDestination, b.mqttFieldDestination);
    TEST_ASSERT_EQUAL_STRING(a.mqttFieldEta, b.mqttFieldEta);
    TEST_ASSERT_EQUAL_STRING(a.mqttFieldTimestamp, b.mqttFieldTimestamp);
    TEST_ASSERT_EQUAL_STRING(a.mqttFieldPlatform, b.mqttFieldPlatform);
    TEST_ASSERT_EQUAL_STRING(a.mqttFieldAC, b.mqttFieldAC);
    TEST_ASSERT_EQUAL_INT(a.refreshInterval, b.refreshInterval);
    TEST_ASSERT_EQUAL_INT(a.numDepartures, b.numDepartures);
    TEST_ASSERT_EQUAL_INT(a.minDepartureTime, b.minDepartureTime);
    TEST_ASSERT_EQUAL_INT(a.brightness, b.brightness);
    TEST_ASSERT_EQUAL_STRING(a.lineColorMap, b.lineColorMap);
    TEST_ASSERT_EQUAL_STRING(a.platformSymbolMap, b.platformSymbolMap);
    TEST_ASSERT_EQUAL_STRING(a.city, b.city);
    TEST_ASSERT_EQUAL_STRING(a.language, b.language);
    TEST_ASSERT_EQUAL(a.debugMode, b.debugMode);
    TEST_ASSERT_EQUAL(a.showPlatform, b.showPlatform);
    TEST_ASSERT_EQUAL(a.scrollEnabled, b.scrollEnabled);
    TEST_ASSERT_EQUAL(a.showMultipleTimes, b.showMultipleTimes);
    TEST_ASSERT_EQUAL_STRING(a.restModePeriods, b.restModePeriods);
    TEST_ASSERT_EQUAL(a.weatherEnabled, b.weatherEnabled);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, a.weatherLatitude, b.weatherLatitude);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, a.weatherLongitude, b.weatherLongitude);
    TEST_ASSERT_EQUAL_INT(a.weatherRefreshInterval, b.weatherRefreshInterval);
    TEST_ASSERT_EQUAL(a.tickerEnabled, b.tickerEnabled);
    TEST_ASSERT_EQUAL_STRING(a.tickerSymbol, b.tickerSymbol);
    TEST_ASSERT_EQUAL_STRING(a.tickerInterval, b.tickerInterval);
    TEST_ASSERT_EQUAL_STRING(a.tickerApiKey, b.tickerApiKey);
    TEST_ASSERT_EQUAL_INT(a.tickerRefreshInterval, b.tickerRefreshInterval);
}

static ConfigImportResult roundTrip(const Config& src, Config& dst, const ConfigImportOptions& opts,
                                    const char* board = THIS_BOARD)
{
    static char buf[CONFIG_JSON_MAX_BYTES];
    const size_t n = configToJson(src, THIS_BOARD, "9", buf, sizeof(buf));
    TEST_ASSERT_TRUE_MESSAGE(n > 0, "configToJson wrote nothing");
    return configFromJson(buf, dst, board, opts);
}

// ------------------------------------------------------------------ round trip

void test_round_trip_preserves_every_ordinary_setting(void)
{
    const Config src = makeConfig();
    Config dst = {};
    dst.geometry = PanelGeometry::Chain2x32;
    const ConfigImportOptions opts = {false, false};
    const ConfigImportResult r = roundTrip(src, dst, opts);

    TEST_ASSERT_EQUAL_INT((int)ConfigImportStatus::Ok, (int)r.status);
    assertSameSettings(src, dst);
}

// The whole point of the plaintext decision: a backup that cannot restore a
// 300-character JWT is not a backup.
void test_secrets_are_exported_in_plaintext(void)
{
    const Config src = makeConfig();
    char buf[CONFIG_JSON_MAX_BYTES];
    TEST_ASSERT_TRUE(configToJson(src, THIS_BOARD, "9", buf, sizeof(buf)) > 0);

    TEST_ASSERT_NOT_NULL(strstr(buf, "s3cr3t-wifi-pw"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "eyJhbGciOiJIUzI1NiJ9.some.jwt-with-an-email"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "mqtt-secret"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "twelve-data-key"));
}

void test_export_stamps_schema_release_and_board(void)
{
    const Config src = makeConfig();
    char buf[CONFIG_JSON_MAX_BYTES];
    TEST_ASSERT_TRUE(configToJson(src, THIS_BOARD, "9", buf, sizeof(buf)) > 0);

    TEST_ASSERT_NOT_NULL(strstr(buf, "\"schema\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"release\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "matrixportal_s3"));
}

// The worst case is not a typical config -- it is one where every string field
// is full. Measuring it here is what lets CONFIG_JSON_MAX_BYTES be a fact
// rather than a guess, and it fails loudly if a field is ever widened.
void test_a_maximally_full_config_still_fits_the_buffer(void)
{
    Config c = makeConfig();
    memset(c.pragueApiKey, 'K', sizeof(c.pragueApiKey) - 1);
    c.pragueApiKey[sizeof(c.pragueApiKey) - 1] = '\0';
    memset(c.pragueStopIds, 'S', sizeof(c.pragueStopIds) - 1);
    c.pragueStopIds[sizeof(c.pragueStopIds) - 1] = '\0';
    memset(c.berlinStopIds, 'B', sizeof(c.berlinStopIds) - 1);
    c.berlinStopIds[sizeof(c.berlinStopIds) - 1] = '\0';
    memset(c.lineColorMap, 'L', sizeof(c.lineColorMap) - 1);
    c.lineColorMap[sizeof(c.lineColorMap) - 1] = '\0';
    memset(c.platformSymbolMap, 'P', sizeof(c.platformSymbolMap) - 1);
    c.platformSymbolMap[sizeof(c.platformSymbolMap) - 1] = '\0';
    memset(c.restModePeriods, 'R', sizeof(c.restModePeriods) - 1);
    c.restModePeriods[sizeof(c.restModePeriods) - 1] = '\0';

    char buf[CONFIG_JSON_MAX_BYTES];
    const size_t n = configToJson(c, THIS_BOARD, "9", buf, sizeof(buf));
    TEST_ASSERT_TRUE_MESSAGE(n > 0, "worst-case config overflowed CONFIG_JSON_MAX_BYTES");

    Config dst = {};
    const ConfigImportOptions opts = {false, false};
    TEST_ASSERT_EQUAL_INT((int)ConfigImportStatus::Ok, (int)configFromJson(buf, dst, THIS_BOARD, opts).status);
    assertSameSettings(c, dst);
}

// ------------------------------------------------------- absent / unknown keys

// The rule the whole importer rests on. A file that predates a field must not
// blank it.
void test_an_absent_key_keeps_the_value_already_on_the_device(void)
{
    Config dst = makeConfig();
    strcpy(dst.wifiSsid, "KeepThisSSID");
    dst.brightness = 200;

    const ConfigImportOptions opts = {false, false};
    const ConfigImportResult r = configFromJson("{\"schema\":1,\"city\":\"Berlin\"}", dst, THIS_BOARD, opts);

    TEST_ASSERT_EQUAL_INT((int)ConfigImportStatus::Ok, (int)r.status);
    TEST_ASSERT_EQUAL_STRING("Berlin", dst.city);
    TEST_ASSERT_EQUAL_STRING("KeepThisSSID", dst.wifiSsid);
    TEST_ASSERT_EQUAL_INT(200, dst.brightness);
}

// What lets an OLD firmware read a NEW export.
void test_unknown_keys_are_ignored_not_rejected(void)
{
    Config dst = makeConfig();
    const ConfigImportOptions opts = {false, false};
    const ConfigImportResult r =
        configFromJson("{\"schema\":1,\"brightness\":77,\"someFutureKey\":{\"a\":[1,2,3]}}", dst, THIS_BOARD, opts);

    TEST_ASSERT_EQUAL_INT((int)ConfigImportStatus::Ok, (int)r.status);
    TEST_ASSERT_EQUAL_INT(77, dst.brightness);
}

// `board` is identity, not a setting. Importing must never rewrite it, which is
// why it is not a Config field at all -- this pins the intent.
void test_board_is_never_written_back_into_the_config(void)
{
    Config dst = makeConfig();
    const ConfigImportOptions opts = {false, false};
    const ConfigImportResult r = configFromJson("{\"schema\":1,\"board\":\"esp32_s3_n8r2\"}", dst, THIS_BOARD, opts);

    TEST_ASSERT_EQUAL_INT((int)ConfigImportStatus::Ok, (int)r.status);
    TEST_ASSERT_TRUE(r.boardMismatch);
    TEST_ASSERT_EQUAL_STRING("esp32_s3_n8r2", r.fileBoard);
}

// -------------------------------------------------------------- failure paths

void test_malformed_json_changes_nothing(void)
{
    Config dst = makeConfig();
    const Config before = dst;
    const ConfigImportOptions opts = {true, true};
    const ConfigImportResult r = configFromJson("{\"schema\":1,\"brightness\":", dst, THIS_BOARD, opts);

    TEST_ASSERT_EQUAL_INT((int)ConfigImportStatus::ParseFailed, (int)r.status);
    assertSameSettings(before, dst);
    TEST_ASSERT_EQUAL_INT(0, r.fieldsApplied);
}

void test_a_json_array_is_not_a_config_document(void)
{
    Config dst = makeConfig();
    const ConfigImportOptions opts = {false, false};
    TEST_ASSERT_EQUAL_INT((int)ConfigImportStatus::NotAnObject, (int)configFromJson("[1,2,3]", dst, THIS_BOARD, opts).status);
}

void test_a_newer_schema_is_refused_rather_than_half_understood(void)
{
    Config dst = makeConfig();
    const Config before = dst;
    const ConfigImportOptions opts = {false, false};
    const ConfigImportResult r = configFromJson("{\"schema\":99,\"brightness\":5}", dst, THIS_BOARD, opts);

    TEST_ASSERT_EQUAL_INT((int)ConfigImportStatus::SchemaTooNew, (int)r.status);
    assertSameSettings(before, dst);
}

// ------------------------------------------------- geometry: opt-in, portable

void test_geometry_is_not_restored_unless_asked(void)
{
    Config src = makeConfig();
    src.geometry = PanelGeometry::Chain2x64;
    src.panelRows = geometryPanelRows(src.geometry);

    Config dst = makeConfig();
    dst.geometry = PanelGeometry::Grid4x32;
    const ConfigImportOptions opts = {false, false};
    const ConfigImportResult r = roundTrip(src, dst, opts);

    TEST_ASSERT_EQUAL_INT((int)ConfigImportStatus::Ok, (int)r.status);
    TEST_ASSERT_FALSE(r.geometryApplied);
    TEST_ASSERT_EQUAL_INT((int)PanelGeometry::Grid4x32, (int)dst.geometry);
}

void test_geometry_is_restored_when_asked(void)
{
    Config src = makeConfig();
    src.geometry = PanelGeometry::Chain2x64;
    src.panelRows = geometryPanelRows(src.geometry);

    Config dst = makeConfig();
    dst.geometry = PanelGeometry::Chain2x32;
    const ConfigImportOptions opts = {true, false};
    const ConfigImportResult r = roundTrip(src, dst, opts);

    TEST_ASSERT_TRUE(r.geometryApplied);
    TEST_ASSERT_EQUAL_INT((int)PanelGeometry::Chain2x64, (int)dst.geometry);
    TEST_ASSERT_EQUAL_INT(geometryPanelRows(PanelGeometry::Chain2x64), dst.panelRows);
}

// Geometry describes the PANELS, so it crosses boards. This is the half of the
// opt-in that a board mismatch does NOT veto.
void test_geometry_restores_even_across_a_board_mismatch(void)
{
    Config src = makeConfig();
    src.geometry = PanelGeometry::Chain2x64;

    Config dst = makeConfig();
    dst.geometry = PanelGeometry::Chain2x32;
    const ConfigImportOptions opts = {true, false};
    const ConfigImportResult r = roundTrip(src, dst, opts, OTHER_BOARD);

    TEST_ASSERT_TRUE(r.boardMismatch);
    TEST_ASSERT_TRUE(r.geometryApplied);
    TEST_ASSERT_EQUAL_INT((int)PanelGeometry::Chain2x64, (int)dst.geometry);
}

// D3: a file written before dispGeom existed carries only panelRows, and must
// land on the arrangement the old UI meant by it.
void test_a_legacy_panelrows_only_file_migrates_to_the_right_arrangement(void)
{
    Config dst = makeConfig();
    dst.geometry = PanelGeometry::Chain2x32;
    const ConfigImportOptions opts = {true, false};
    const ConfigImportResult r = configFromJson("{\"schema\":1,\"panelRows\":2}", dst, THIS_BOARD, opts);

    TEST_ASSERT_EQUAL_INT((int)ConfigImportStatus::Ok, (int)r.status);
    TEST_ASSERT_TRUE(r.geometryApplied);
    TEST_ASSERT_EQUAL_INT((int)geometryFromLegacyPanelRows(2), (int)dst.geometry);
    TEST_ASSERT_EQUAL_INT((int)PanelGeometry::Grid4x32, (int)dst.geometry);
}

// dispGeom is authoritative; panelRows is derived and lossy, so a file carrying
// both must not let the lossy one win.
void test_dispgeom_wins_over_a_conflicting_legacy_panelrows(void)
{
    Config dst = makeConfig();
    const ConfigImportOptions opts = {true, false};
    const ConfigImportResult r =
        configFromJson("{\"schema\":1,\"dispGeom\":3,\"panelRows\":2}", dst, THIS_BOARD, opts);

    TEST_ASSERT_EQUAL_INT((int)ConfigImportStatus::Ok, (int)r.status);
    TEST_ASSERT_EQUAL_INT((int)PanelGeometry::Chain2x64, (int)dst.geometry);
}

// --------------------------------------------- wiring: opt-in, board-specific

void test_wiring_is_not_restored_unless_asked(void)
{
    const Config src = makeConfig();
    Config dst = makeConfig();
    dst.hwProfile.pins.r1 = 42;
    dst.hwProfile.order = RgbOrder::RGB;
    const ConfigImportOptions opts = {false, false};
    const ConfigImportResult r = roundTrip(src, dst, opts);

    TEST_ASSERT_FALSE(r.wiringApplied);
    TEST_ASSERT_EQUAL_INT(42, dst.hwProfile.pins.r1);
    TEST_ASSERT_EQUAL_INT((int)RgbOrder::RGB, (int)dst.hwProfile.order);
}

void test_wiring_is_restored_when_asked_on_the_same_board(void)
{
    const Config src = makeConfig(); // r1 == 10, order GBR, driver 3
    Config dst = makeConfig();
    dst.hwProfile.pins.r1 = 42;
    dst.hwProfile.order = RgbOrder::RGB;
    dst.hwProfile.driver = 0;
    const ConfigImportOptions opts = {false, true};
    const ConfigImportResult r = roundTrip(src, dst, opts);

    TEST_ASSERT_TRUE(r.wiringApplied);
    TEST_ASSERT_FALSE(r.wiringRefused);
    TEST_ASSERT_EQUAL_INT(10, dst.hwProfile.pins.r1);
    TEST_ASSERT_EQUAL_INT((int)RgbOrder::GBR, (int)dst.hwProfile.order);
    TEST_ASSERT_EQUAL_INT(3, dst.hwProfile.driver);
    TEST_ASSERT_TRUE(dst.hwProfile.useCustomPins);
}

// The one hard veto: GPIOs describe THIS controller. Asking for them from
// another board's file is refused even with the opt-in checked.
void test_wiring_is_refused_across_a_board_mismatch_even_when_asked(void)
{
    const Config src = makeConfig();
    Config dst = makeConfig();
    dst.hwProfile.pins.r1 = 42;
    dst.hwProfile.order = RgbOrder::RGB;
    const ConfigImportOptions opts = {false, true};
    const ConfigImportResult r = roundTrip(src, dst, opts, OTHER_BOARD);

    TEST_ASSERT_TRUE(r.boardMismatch);
    TEST_ASSERT_TRUE(r.wiringRefused);
    TEST_ASSERT_FALSE(r.wiringApplied);
    TEST_ASSERT_EQUAL_INT(42, dst.hwProfile.pins.r1);
    TEST_ASSERT_EQUAL_INT((int)RgbOrder::RGB, (int)dst.hwProfile.order);
}

// A board mismatch vetoes the WIRING, not the config. Everything else lands.
void test_a_board_mismatch_still_imports_every_ordinary_setting(void)
{
    const Config src = makeConfig();
    Config dst = {};
    dst.geometry = PanelGeometry::Chain2x32;
    const ConfigImportOptions opts = {false, false};
    const ConfigImportResult r = roundTrip(src, dst, opts, OTHER_BOARD);

    TEST_ASSERT_EQUAL_INT((int)ConfigImportStatus::Ok, (int)r.status);
    TEST_ASSERT_TRUE(r.boardMismatch);
    assertSameSettings(src, dst);
}

// A stored map that validation rejects must never reach the driver, so it must
// not survive an import either.
void test_an_invalid_pin_map_is_not_imported(void)
{
    Config src = makeConfig();
    src.hwProfile.pins.r1 = src.hwProfile.pins.g1; // duplicate -> invalid
    Config dst = makeConfig();
    dst.hwProfile.pins.r1 = 42;
    const ConfigImportOptions opts = {false, true};
    const ConfigImportResult r = roundTrip(src, dst, opts);

    TEST_ASSERT_FALSE(r.wiringApplied);
    TEST_ASSERT_EQUAL_INT(42, dst.hwProfile.pins.r1);
}

// ------------------------------------------------------------------- clamping

void test_clamp_forces_every_field_into_the_boot_time_range(void)
{
    Config c = makeConfig();
    c.refreshInterval = 5000;
    c.minDepartureTime = -4;
    c.brightness = 900;
    c.weatherRefreshInterval = 1;   // below the floor
    c.tickerRefreshInterval = 99999; // above the ceiling
    c.hwProfile.driver = 77;
    configClamp(c);

    TEST_ASSERT_EQUAL_INT(300, c.refreshInterval);
    TEST_ASSERT_EQUAL_INT(0, c.minDepartureTime);
    TEST_ASSERT_EQUAL_INT(255, c.brightness);
    TEST_ASSERT_EQUAL_INT(5, c.weatherRefreshInterval);
    TEST_ASSERT_EQUAL_INT(600, c.tickerRefreshInterval);
    TEST_ASSERT_EQUAL_INT(5, c.hwProfile.driver);

    // The other side of each two-sided range, so a clamp that only ever
    // enforced one bound cannot pass.
    c.refreshInterval = 1;
    c.brightness = -9;
    c.weatherRefreshInterval = 9999;
    c.tickerRefreshInterval = 1;
    configClamp(c);
    TEST_ASSERT_EQUAL_INT(10, c.refreshInterval);
    TEST_ASSERT_EQUAL_INT(0, c.brightness);
    TEST_ASSERT_EQUAL_INT(120, c.weatherRefreshInterval);
    TEST_ASSERT_EQUAL_INT(120, c.tickerRefreshInterval);
}

// numDepartures is the one clamp that depends on ANOTHER field, so importing a
// 7-row config onto a 128x32 panel must not leave 7 behind.
void test_clamp_bounds_departure_rows_by_the_current_geometry(void)
{
    Config c = makeConfig();
    c.geometry = PanelGeometry::Chain2x32;
    c.numDepartures = 7;
    configClamp(c);
    TEST_ASSERT_EQUAL_INT(geometryMaxDepartureRows(PanelGeometry::Chain2x32), c.numDepartures);

    c.geometry = PanelGeometry::Chain2x64;
    c.numDepartures = 7;
    configClamp(c);
    TEST_ASSERT_EQUAL_INT(7, c.numDepartures);
}

// mqttPort was clamped on the web-save path but nowhere in configClamp, so an
// import could store a port no form would ever have accepted.
void test_clamp_bounds_the_mqtt_port(void)
{
    Config c = makeConfig();
    c.mqttPort = -5;
    configClamp(c);
    TEST_ASSERT_EQUAL_INT(1, c.mqttPort);

    c.mqttPort = 999999;
    configClamp(c);
    TEST_ASSERT_EQUAL_INT(65535, c.mqttPort);
}

// The web form advertises min=5 max=120 and loadConfig has always honoured that,
// but parseOptionalSettings clamped saves to 10..60 -- so the UI offered a range
// it did not keep. configClamp is the definition; pin it.
void test_clamp_honours_the_weather_refresh_range_the_form_advertises(void)
{
    Config c = makeConfig();
    c.weatherRefreshInterval = 90; // inside 5..120, outside the old 10..60
    configClamp(c);
    TEST_ASSERT_EQUAL_INT(90, c.weatherRefreshInterval);

    c.weatherRefreshInterval = 5; // the form's minimum must survive
    configClamp(c);
    TEST_ASSERT_EQUAL_INT(5, c.weatherRefreshInterval);
}

void test_clamp_restores_an_empty_line_colour_map_to_the_default(void)
{
    Config c = makeConfig();
    c.lineColorMap[0] = '\0';
    configClamp(c);
    TEST_ASSERT_EQUAL_STRING(DEFAULT_LINE_COLOR_MAP, c.lineColorMap);
}

void test_clamp_is_idempotent(void)
{
    Config a = makeConfig();
    a.brightness = 900;
    a.numDepartures = 99;
    configClamp(a);
    // Assert the first pass actually MOVED something. Without this the test
    // passes against a no-op clamp, which is exactly what it exists to catch.
    TEST_ASSERT_EQUAL_INT(255, a.brightness);
    TEST_ASSERT_EQUAL_INT(geometryMaxDepartureRows(a.geometry), a.numDepartures);

    Config b = a;
    configClamp(b);
    assertSameSettings(a, b);
    TEST_ASSERT_EQUAL_INT((int)a.geometry, (int)b.geometry);
}

void test_clamp_repairs_an_out_of_range_geometry(void)
{
    Config c = makeConfig();
    c.geometry = (PanelGeometry)99;
    configClamp(c);
    TEST_ASSERT_EQUAL_INT((int)PanelGeometry::Chain2x32, (int)c.geometry);
    TEST_ASSERT_EQUAL_INT(geometryPanelRows(PanelGeometry::Chain2x32), c.panelRows);
}

// The acceptance criterion, made executable: whatever a file contains, the
// result cannot be a config a boot-time load would have rejected.
void test_no_import_can_produce_an_out_of_range_config(void)
{
    Config dst = makeConfig();
    const ConfigImportOptions opts = {true, false};
    const ConfigImportResult r = configFromJson(
        "{\"schema\":1,\"brightness\":9999,\"refreshInterval\":-40,\"numDepartures\":99,"
        "\"dispGeom\":1,\"tickerRefreshInterval\":0}",
        dst, THIS_BOARD, opts);

    TEST_ASSERT_EQUAL_INT((int)ConfigImportStatus::Ok, (int)r.status);
    TEST_ASSERT_EQUAL_INT(255, dst.brightness);
    TEST_ASSERT_EQUAL_INT(10, dst.refreshInterval);
    TEST_ASSERT_EQUAL_INT(geometryMaxDepartureRows(PanelGeometry::Chain2x32), dst.numDepartures);
    TEST_ASSERT_EQUAL_INT(120, dst.tickerRefreshInterval);
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_round_trip_preserves_every_ordinary_setting);
    RUN_TEST(test_secrets_are_exported_in_plaintext);
    RUN_TEST(test_export_stamps_schema_release_and_board);
    RUN_TEST(test_a_maximally_full_config_still_fits_the_buffer);
    RUN_TEST(test_an_absent_key_keeps_the_value_already_on_the_device);
    RUN_TEST(test_unknown_keys_are_ignored_not_rejected);
    RUN_TEST(test_board_is_never_written_back_into_the_config);
    RUN_TEST(test_malformed_json_changes_nothing);
    RUN_TEST(test_a_json_array_is_not_a_config_document);
    RUN_TEST(test_a_newer_schema_is_refused_rather_than_half_understood);
    RUN_TEST(test_geometry_is_not_restored_unless_asked);
    RUN_TEST(test_geometry_is_restored_when_asked);
    RUN_TEST(test_geometry_restores_even_across_a_board_mismatch);
    RUN_TEST(test_a_legacy_panelrows_only_file_migrates_to_the_right_arrangement);
    RUN_TEST(test_dispgeom_wins_over_a_conflicting_legacy_panelrows);
    RUN_TEST(test_wiring_is_not_restored_unless_asked);
    RUN_TEST(test_wiring_is_restored_when_asked_on_the_same_board);
    RUN_TEST(test_wiring_is_refused_across_a_board_mismatch_even_when_asked);
    RUN_TEST(test_a_board_mismatch_still_imports_every_ordinary_setting);
    RUN_TEST(test_an_invalid_pin_map_is_not_imported);
    RUN_TEST(test_clamp_forces_every_field_into_the_boot_time_range);
    RUN_TEST(test_clamp_bounds_departure_rows_by_the_current_geometry);
    RUN_TEST(test_clamp_bounds_the_mqtt_port);
    RUN_TEST(test_clamp_honours_the_weather_refresh_range_the_form_advertises);
    RUN_TEST(test_clamp_restores_an_empty_line_colour_map_to_the_default);
    RUN_TEST(test_clamp_is_idempotent);
    RUN_TEST(test_clamp_repairs_an_out_of_range_geometry);
    RUN_TEST(test_no_import_can_produce_an_out_of_range_config);
    return UNITY_END();
}
