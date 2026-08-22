// Asset-name matcher: the code that decides whether this device flashes a given
// firmware binary. Getting it wrong means installing another board's image, so
// it is tested exhaustively here where a run costs nothing.
#include <unity.h>
#include <string.h>

#include "../../src/network/OtaAssetName.h"
#include "../../src/network/OtaAssetName.cpp"

static const char* MP = "matrixportal_s3";
static const char* N8 = "esp32_s3_n8r2";

// ---------------------------------------------------------------- bare names

void test_bare_matches_own_board(void)
{
    OtaAssetInfo i = otaClassifyAsset("spojboard-matrixportal_s3-r9-1a2b3c4d.bin", MP);
    TEST_ASSERT_TRUE(i.match == OtaAssetMatch::Bare);
    TEST_ASSERT_EQUAL_INT(9, i.release);
    TEST_ASSERT_EQUAL_STRING("", i.display);
}

// The underscore trap: a naive "split on the last _" reads this board's own
// bare name as board=esp32_s3 + display=n8r2.
void test_bare_underscored_board_is_not_split(void)
{
    OtaAssetInfo i = otaClassifyAsset("spojboard-esp32_s3_n8r2-r9-1a2b3c4d.bin", N8);
    TEST_ASSERT_TRUE(i.match == OtaAssetMatch::Bare);
    TEST_ASSERT_EQUAL_STRING("", i.display);
    TEST_ASSERT_EQUAL_INT(9, i.release);
}

void test_other_board_rejected(void)
{
    TEST_ASSERT_TRUE(otaClassifyAsset("spojboard-esp32_s3_n8r2-r9-1a2b3c4d.bin", MP).match
                     == OtaAssetMatch::None);
    TEST_ASSERT_TRUE(otaClassifyAsset("spojboard-matrixportal_s3-r9-1a2b3c4d.bin", N8).match
                     == OtaAssetMatch::None);
}

// A board name must match whole, not as a prefix.
void test_board_prefix_is_not_a_match(void)
{
    TEST_ASSERT_TRUE(otaClassifyAsset("spojboard-matrixportal_s3x-r9-1a2b3c4d.bin", MP).match
                     == OtaAssetMatch::None);
    TEST_ASSERT_TRUE(otaClassifyAsset("spojboard-esp32_s3-r9-1a2b3c4d.bin", N8).match
                     == OtaAssetMatch::None);
}

// ------------------------------------------------------------- display names

void test_display_suffix_parsed(void)
{
    OtaAssetInfo i = otaClassifyAsset("spojboard-matrixportal_s3_2x32-r10-1a2b3c4d.bin", MP);
    TEST_ASSERT_TRUE(i.match == OtaAssetMatch::Display);
    TEST_ASSERT_EQUAL_STRING("2x32", i.display);
    TEST_ASSERT_EQUAL_INT(10, i.release);

    OtaAssetInfo j = otaClassifyAsset("spojboard-esp32_s3_n8r2_2x64-r10-1a2b3c4d.bin", N8);
    TEST_ASSERT_TRUE(j.match == OtaAssetMatch::Display);
    TEST_ASSERT_EQUAL_STRING("2x64", j.display);
}

void test_display_suffix_of_other_board_rejected(void)
{
    TEST_ASSERT_TRUE(otaClassifyAsset("spojboard-esp32_s3_n8r2_2x32-r10-1a2b3c4d.bin", MP).match
                     == OtaAssetMatch::None);
}

// THE "-r" TRAP. A suffix starting with 'r' must never let the board name
// truncate. Left-to-right "first -r" would read board="matrixportal_s3" here
// and accept the file.
void test_r_prefixed_suffix_does_not_truncate_board(void)
{
    OtaAssetInfo i = otaClassifyAsset("spojboard-matrixportal_s3_rgb-r10-1a2b3c4d.bin", MP);
    TEST_ASSERT_TRUE(i.match == OtaAssetMatch::None);
}

// Only <digits>x<digits> counts as a display token; anything else is not ours.
void test_non_display_suffix_rejected(void)
{
    const char* bad[] = {
        "spojboard-matrixportal_s3_beta-r10-1a2b3c4d.bin",
        "spojboard-matrixportal_s3_2x-r10-1a2b3c4d.bin",
        "spojboard-matrixportal_s3_x32-r10-1a2b3c4d.bin",
        "spojboard-matrixportal_s3_-r10-1a2b3c4d.bin",
    };
    for (unsigned k = 0; k < sizeof(bad) / sizeof(bad[0]); k++)
        TEST_ASSERT_TRUE(otaClassifyAsset(bad[k], MP).match == OtaAssetMatch::None);
}

// ------------------------------------------------------------------- details

void test_dirty_build_id_still_parses(void)
{
    OtaAssetInfo i = otaClassifyAsset("spojboard-matrixportal_s3-r9-1a2b3c4d-dirty.bin", MP);
    TEST_ASSERT_TRUE(i.match == OtaAssetMatch::Bare);
    TEST_ASSERT_EQUAL_INT(9, i.release);
}

void test_multi_digit_release(void)
{
    TEST_ASSERT_EQUAL_INT(10, otaClassifyAsset("spojboard-matrixportal_s3-r10-1a2b3c4d.bin", MP).release);
    TEST_ASSERT_EQUAL_INT(123, otaClassifyAsset("spojboard-matrixportal_s3-r123-1a2b3c4d.bin", MP).release);
}

// A build id that itself contains "-r..." must not be mistaken for the marker.
void test_release_marker_is_rightmost_valid_one(void)
{
    OtaAssetInfo i = otaClassifyAsset("spojboard-matrixportal_s3-r9-1a2b3c4d.bin", MP);
    TEST_ASSERT_EQUAL_INT(9, i.release);
}

void test_legacy_and_junk_rejected(void)
{
    const char* bad[] = {
        "spojboard-r4-1a2b3c4d.bin",             // pre-variant legacy name
        "spojboard-matrixportal_s3-r9-1a2b3c4d", // no .bin
        "beerboard-matrixportal_s3-r9-1a2b.bin", // not ours
        "spojboard-matrixportal_s3-rX-1a2b.bin", // no digits after -r
        "spojboard-.bin",
        "",
    };
    for (unsigned k = 0; k < sizeof(bad) / sizeof(bad[0]); k++)
        TEST_ASSERT_TRUE(otaClassifyAsset(bad[k], MP).match == OtaAssetMatch::None);
    TEST_ASSERT_TRUE(otaClassifyAsset(nullptr, MP).match == OtaAssetMatch::None);
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_bare_matches_own_board);
    RUN_TEST(test_bare_underscored_board_is_not_split);
    RUN_TEST(test_other_board_rejected);
    RUN_TEST(test_board_prefix_is_not_a_match);
    RUN_TEST(test_display_suffix_parsed);
    RUN_TEST(test_display_suffix_of_other_board_rejected);
    RUN_TEST(test_r_prefixed_suffix_does_not_truncate_board);
    RUN_TEST(test_non_display_suffix_rejected);
    RUN_TEST(test_dirty_build_id_still_parses);
    RUN_TEST(test_multi_digit_release);
    RUN_TEST(test_release_marker_is_rightmost_valid_one);
    RUN_TEST(test_legacy_and_junk_rejected);
    return UNITY_END();
}
