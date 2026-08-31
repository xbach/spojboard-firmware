// Release-asset selection policy (TA-0303 follow-up). Until now only the
// ingredient -- otaClassifyAsset -- was testable; the two-pass loop that decides
// WHICH assets a device is offered, and in what order, lived behind network
// headers and was never exercised off-device.
//
// The case that matters most is the ordinary one: an r10 release publishes ONE
// bare asset per board, because panel arrangement is a runtime setting.
#include <unity.h>
#include <ArduinoJson.h>
#include <string.h>

#include "../../src/network/OtaAssetName.h"
#include "../../src/network/OtaAssetName.cpp"
#include "../../src/network/OtaAssetSelect.h"
#include "../../src/network/OtaAssetSelect.cpp"

static const char* MP = "matrixportal_s3";
static const char* N8 = "esp32_s3_n8r2";

// Exactly what CI publishes: one binary per board, no display field.
static const char* R10_RELEASE = R"({
  "tag_name": "r10",
  "assets": [
    {"name":"spojboard-matrixportal_s3-r10-1a2b3c4d.bin",
     "browser_download_url":"https://example.invalid/mp.bin","size":1516455},
    {"name":"spojboard-esp32_s3_n8r2-r10-1a2b3c4d.bin",
     "browser_download_url":"https://example.invalid/n8.bin","size":1498112}
  ]})";

void test_base_release_offers_this_board_exactly_one_asset(void)
{
    DynamicJsonDocument doc(4096);
    TEST_ASSERT_TRUE(deserializeJson(doc, R10_RELEASE) == DeserializationError::Ok);

    OtaAssetOption opts[6];
    const int n = otaCollectAssetOptions(doc, MP, opts, 6);

    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_EQUAL_STRING("spojboard-matrixportal_s3-r10-1a2b3c4d.bin", opts[0].name);
    TEST_ASSERT_EQUAL_STRING("https://example.invalid/mp.bin", opts[0].url);
    TEST_ASSERT_EQUAL_STRING("", opts[0].display); // bare: drives every arrangement
    TEST_ASSERT_EQUAL_UINT32(1516455, (uint32_t)opts[0].size);
}

// The other board's binary must never be offered. Board mismatch is the one
// hard rejection: a wrong-board image means a different pin map and a dead
// display with nothing left to diagnose it on.
void test_the_other_boards_asset_is_never_offered(void)
{
    DynamicJsonDocument doc(4096);
    deserializeJson(doc, R10_RELEASE);

    OtaAssetOption opts[6];
    const int n = otaCollectAssetOptions(doc, N8, opts, 6);

    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_EQUAL_STRING("spojboard-esp32_s3_n8r2-r10-1a2b3c4d.bin", opts[0].name);
}

// A release that publishes a SECOND flavour for one board -- the reuse case.
// Nothing emits a display token today, but the mechanism is what a build that
// cannot be expressed at runtime would use, so the ordering must hold: the
// specific asset is offered first, the bare one remains as the fallback.
static const char* MIXED_RELEASE = R"({
  "tag_name": "r11",
  "assets": [
    {"name":"spojboard-matrixportal_s3-r11-1a2b3c4d.bin",
     "browser_download_url":"https://example.invalid/mp-bare.bin","size":1500000},
    {"name":"spojboard-esp32_s3_n8r2-r11-1a2b3c4d.bin",
     "browser_download_url":"https://example.invalid/n8-bare.bin","size":1490000},
    {"name":"spojboard-matrixportal_s3-4x32-r11-1a2b3c4d.bin",
     "browser_download_url":"https://example.invalid/mp-4x32.bin","size":1510000}
  ]})";

void test_a_specific_build_is_offered_before_the_bare_one(void)
{
    DynamicJsonDocument doc(4096);
    deserializeJson(doc, MIXED_RELEASE);

    OtaAssetOption opts[6];
    const int n = otaCollectAssetOptions(doc, MP, opts, 6);

    // Both of this board's assets, specific first -- even though the bare one
    // appears earlier in the release's own asset order.
    TEST_ASSERT_EQUAL_INT(2, n);
    TEST_ASSERT_EQUAL_STRING("4x32", opts[0].display);
    TEST_ASSERT_EQUAL_STRING("", opts[1].display);
    TEST_ASSERT_EQUAL_STRING("https://example.invalid/mp-4x32.bin", opts[0].url);
}

void test_a_mixed_release_still_excludes_the_other_board(void)
{
    DynamicJsonDocument doc(4096);
    deserializeJson(doc, MIXED_RELEASE);

    OtaAssetOption opts[6];
    const int n = otaCollectAssetOptions(doc, N8, opts, 6);

    TEST_ASSERT_EQUAL_INT(1, n);
    TEST_ASSERT_EQUAL_STRING("spojboard-esp32_s3_n8r2-r11-1a2b3c4d.bin", opts[0].name);
}

// ReleaseInfo is ~3.1KB and lives on a task stack budget, so the cap is a
// memory bound, not a formality.
void test_the_option_cap_is_respected(void)
{
    DynamicJsonDocument doc(4096);
    deserializeJson(doc, MIXED_RELEASE);

    OtaAssetOption opts[1];
    TEST_ASSERT_EQUAL_INT(1, otaCollectAssetOptions(doc, MP, opts, 1));
    TEST_ASSERT_EQUAL_STRING("4x32", opts[0].display);
}

// An asset list that is missing or malformed yields no options rather than
// garbage -- the caller then reports "no firmware for this hardware".
void test_a_release_with_no_assets_yields_nothing(void)
{
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, R"({"tag_name":"r11"})");

    OtaAssetOption opts[6];
    TEST_ASSERT_EQUAL_INT(0, otaCollectAssetOptions(doc, MP, opts, 6));
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_base_release_offers_this_board_exactly_one_asset);
    RUN_TEST(test_the_other_boards_asset_is_never_offered);
    RUN_TEST(test_a_specific_build_is_offered_before_the_bare_one);
    RUN_TEST(test_a_mixed_release_still_excludes_the_other_board);
    RUN_TEST(test_the_option_cap_is_respected);
    RUN_TEST(test_a_release_with_no_assets_yields_nothing);
    return UNITY_END();
}
