// Asset-name matcher: the code that decides whether this device flashes a given
// firmware binary. Getting it wrong means installing another board's image, so
// it is tested exhaustively here where a run costs nothing.
#include <unity.h>
#include <string.h>
#include <stdio.h>

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

// Board names contain underscores. With dash-separated fields this is a
// non-event, but it was the whole difficulty under the old <board>_<display>
// packing, so keep asserting it.
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
    OtaAssetInfo i = otaClassifyAsset("spojboard-matrixportal_s3-2x32-r10-1a2b3c4d.bin", MP);
    TEST_ASSERT_TRUE(i.match == OtaAssetMatch::Display);
    TEST_ASSERT_EQUAL_STRING("2x32", i.display);
    TEST_ASSERT_EQUAL_INT(10, i.release);

    OtaAssetInfo j = otaClassifyAsset("spojboard-esp32_s3_n8r2-2x64-r10-1a2b3c4d.bin", N8);
    TEST_ASSERT_TRUE(j.match == OtaAssetMatch::Display);
    TEST_ASSERT_EQUAL_STRING("2x64", j.display);
}

void test_display_suffix_of_other_board_rejected(void)
{
    TEST_ASSERT_TRUE(otaClassifyAsset("spojboard-esp32_s3_n8r2-2x32-r10-1a2b3c4d.bin", MP).match
                     == OtaAssetMatch::None);
}

// THE "-r" TRAP. r8 takes the text up to the FIRST "-r", so it reads this as
// board="matrixportal_s3" and accepts it. Our parser identifies the release
// field by position and exact r<digits> shape, so "rgb" is just an unknown
// field and the name is rejected.
void test_r_prefixed_suffix_does_not_truncate_board(void)
{
    OtaAssetInfo i = otaClassifyAsset("spojboard-matrixportal_s3-rgb-r10-1a2b3c4d.bin", MP);
    TEST_ASSERT_TRUE(i.match == OtaAssetMatch::None);
}

// Field 1 must be either r<digits> or a <digits>x<digits> geometry token.
// Anything else is a name we did not produce.
void test_non_display_suffix_rejected(void)
{
    const char* bad[] = {
        "spojboard-matrixportal_s3-beta-r10-1a2b3c4d.bin",
        "spojboard-matrixportal_s3-2x-r10-1a2b3c4d.bin",
        "spojboard-matrixportal_s3-x32-r10-1a2b3c4d.bin",
        "spojboard-matrixportal_s3--r10-1a2b3c4d.bin",
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

// ------------------------------------------------- selection policy ordering

// Mirrors GitHubOTA::collectAssetOptions: geometry builds first, then bare, so
// options[0] is the most specific build available and a caller that ignores the
// rest still behaves sensibly.
static int collectPolicy(const char* const* names, int n, const char* board,
                         const char** out, int maxOut)
{
    int count = 0;
    for (int pass = 0; pass < 2 && count < maxOut; pass++)
    {
        OtaAssetMatch want = (pass == 0) ? OtaAssetMatch::Display : OtaAssetMatch::Bare;
        for (int i = 0; i < n && count < maxOut; i++)
            if (otaClassifyAsset(names[i], board).match == want)
                out[count++] = names[i];
    }
    return count;
}

void test_geometry_builds_are_offered_before_bare(void)
{
    // Bare deliberately listed FIRST, as upload order might well put it there.
    const char* release[] = {
        "spojboard-matrixportal_s3-r10-1a2b3c4d.bin",
        "spojboard-matrixportal_s3-2x32-r10-1a2b3c4d.bin",
        "spojboard-matrixportal_s3-4x32-r10-1a2b3c4d.bin",
        "spojboard-esp32_s3_n8r2-r10-1a2b3c4d.bin",
        "spojboard-esp32_s3_n8r2-2x32-r10-1a2b3c4d.bin",
    };
    const char* got[6];
    int n = collectPolicy(release, 5, MP, got, 6);

    TEST_ASSERT_EQUAL_INT(3, n); // the other board's two are excluded
    TEST_ASSERT_EQUAL_STRING("spojboard-matrixportal_s3-2x32-r10-1a2b3c4d.bin", got[0]);
    TEST_ASSERT_EQUAL_STRING("spojboard-matrixportal_s3-4x32-r10-1a2b3c4d.bin", got[1]);
    TEST_ASSERT_EQUAL_STRING("spojboard-matrixportal_s3-r10-1a2b3c4d.bin", got[2]);
}

// A release with no geometry builds still updates every device.
void test_bare_only_release_yields_one_option(void)
{
    const char* release[] = {
        "spojboard-matrixportal_s3-r10-1a2b3c4d.bin",
        "spojboard-esp32_s3_n8r2-r10-1a2b3c4d.bin",
    };
    const char* got[6];
    TEST_ASSERT_EQUAL_INT(1, collectPolicy(release, 2, MP, got, 6));
    TEST_ASSERT_EQUAL_STRING("spojboard-matrixportal_s3-r10-1a2b3c4d.bin", got[0]);
}

// Board lock is the one hard gate: a release entirely for the other board
// offers nothing, however many geometries it carries.
void test_other_board_release_offers_nothing(void)
{
    const char* release[] = {
        "spojboard-esp32_s3_n8r2-r10-1a2b3c4d.bin",
        "spojboard-esp32_s3_n8r2-2x32-r10-1a2b3c4d.bin",
        "spojboard-esp32_s3_n8r2-4x32-r10-1a2b3c4d.bin",
    };
    const char* got[6];
    TEST_ASSERT_EQUAL_INT(0, collectPolicy(release, 3, MP, got, 6));
}

// ------------------------------------------- shipped display tokens (TA-0269)
//
// The tests above prove the GRAMMAR handles display tokens. This one pins the
// TOKENS THE BUILD ACTUALLY EMITS -- platformio.ini's custom_display_variant
// values -- against that grammar, so the producer and the consumer cannot drift
// apart silently. Adding a fourth geometry means adding it here.
//
// Keep in sync with: platformio.ini (custom_display_variant) and the
// DISPLAY_VARIANT block in src/config/AppConfig.h.
static const char* SHIPPED_DISPLAY_TOKENS[] = {"2x32", "4x32", "2x64"};
static const int SHIPPED_DISPLAY_TOKEN_COUNT = 3;

// THE PERMANENT NAMING CONSTRAINT. r8's parser reads the board field as the text
// up to the FIRST "-r", so a display token beginning with 'r' truncates the board
// name and makes an r8 device accept another board's firmware -- a silent
// mis-flash. r9+ is immune by construction, but r8 devices are in the field and
// cannot be changed, so this binds forever. scripts/post_build.py refuses such a
// token at build time; this asserts the rule itself so it survives a refactor of
// that script. test_r8compat proves the trap is real against r8's own parser.
void test_no_shipped_display_token_begins_with_r(void)
{
    for (int i = 0; i < SHIPPED_DISPLAY_TOKEN_COUNT; i++)
    {
        TEST_ASSERT_TRUE_MESSAGE(SHIPPED_DISPLAY_TOKENS[i][0] != 'r',
                                 "display token must not begin with 'r' (r8 mis-flash trap)");
    }
}

void test_shipped_display_tokens_round_trip(void)
{
    for (int i = 0; i < SHIPPED_DISPLAY_TOKEN_COUNT; i++)
    {
        char name[96];
        snprintf(name, sizeof(name), "spojboard-matrixportal_s3-%s-r10-1a2b3c4d.bin",
                 SHIPPED_DISPLAY_TOKENS[i]);

        OtaAssetInfo mine = otaClassifyAsset(name, MP);
        TEST_ASSERT_TRUE(mine.match == OtaAssetMatch::Display);
        TEST_ASSERT_EQUAL_STRING(SHIPPED_DISPLAY_TOKENS[i], mine.display);
        TEST_ASSERT_EQUAL_INT(10, mine.release);

        // The board gate is what stops a wrong-pin-map flash, and it must hold
        // for every geometry, not just the ones that existed when it was written.
        OtaAssetInfo theirs = otaClassifyAsset(name, N8);
        TEST_ASSERT_TRUE(theirs.match == OtaAssetMatch::None);
    }
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_no_shipped_display_token_begins_with_r);
    RUN_TEST(test_shipped_display_tokens_round_trip);
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
    RUN_TEST(test_geometry_builds_are_offered_before_bare);
    RUN_TEST(test_bare_only_release_yields_one_option);
    RUN_TEST(test_other_board_release_offers_nothing);
    return UNITY_END();
}
