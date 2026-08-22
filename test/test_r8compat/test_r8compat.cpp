// Will an r8 device in the field accept the assets we are about to publish?
//
// The function below is a FROZEN, VERBATIM COPY of extractVariantFromFilename
// from the r8 tag (`git show r8:src/network/GitHubOTA.cpp`), together with r8's
// strcmp check. It is the parser running on every device already shipped.
//
// DO NOT "FIX" OR TIDY IT. Its bugs are the point -- this suite exists to
// predict the behaviour of firmware we can no longer change. If the real r8
// source and this copy ever disagree, this copy is wrong.
//
// Verified against r8 on 2026-08-22 with:
//     git show r8:src/network/GitHubOTA.cpp
#include <unity.h>
#include <string.h>
#include <stdlib.h>

static bool r8_extractVariant(const char* filename, char* variant, size_t variantSize)
{
    // Expected format: spojboard-{variant}-r{number}-{8hex}.bin
    // Example: spojboard-matrixportal_s3-r4-a1b2c3d4.bin

    if (!filename || !variant)
    {
        return false;
    }

    // Find "spojboard-" prefix
    const char* start = strstr(filename, "spojboard-");
    if (!start)
    {
        return false;
    }

    start += 10; // Skip "spojboard-"

    // Find next "-r" which marks end of variant
    const char* end = strstr(start, "-r");
    if (!end)
    {
        return false;
    }

    // Extract variant name
    int len = end - start;
    if (len <= 0 || (size_t)len >= variantSize)
    {
        return false;
    }

    strncpy(variant, start, len);
    variant[len] = '\0';
    return true;
}

// r8's accept/reject decision for a .bin asset, minus the Serial logging.
static bool r8_accepts(const char* filename, const char* variantName)
{
    char fileVariant[32] = {0};
    if (r8_extractVariant(filename, fileVariant, sizeof(fileVariant)))
        return strcmp(fileVariant, variantName) == 0;

    // r8's legacy fallback: "spojboard-r<digit>..." is assumed matrixportal_s3
    const char* afterPrefix = filename + 10;
    if (afterPrefix[0] == 'r' && afterPrefix[1] >= '0' && afterPrefix[1] <= '9')
        return strcmp(variantName, "matrixportal_s3") == 0;

    return false;
}

static const char* MP = "matrixportal_s3";
static const char* N8 = "esp32_s3_n8r2";

// THE LOAD-BEARING ASSERTION: r9 keeps r8's naming, so r8 devices can install it.
void test_r8_accepts_r9_bare_assets(void)
{
    TEST_ASSERT_TRUE(r8_accepts("spojboard-matrixportal_s3-r9-1a2b3c4d.bin", MP));
    TEST_ASSERT_TRUE(r8_accepts("spojboard-esp32_s3_n8r2-r9-1a2b3c4d.bin", N8));
}

void test_r8_still_rejects_the_other_board(void)
{
    TEST_ASSERT_FALSE(r8_accepts("spojboard-esp32_s3_n8r2-r9-1a2b3c4d.bin", MP));
    TEST_ASSERT_FALSE(r8_accepts("spojboard-matrixportal_s3-r9-1a2b3c4d.bin", N8));
}

// If r10 carries display-suffixed assets, r8 devices must ignore them rather
// than flash one. They extract "matrixportal_s3_2x32", which != VARIANT_NAME.
void test_r8_ignores_display_suffixed_assets(void)
{
    TEST_ASSERT_FALSE(r8_accepts("spojboard-matrixportal_s3_2x32-r10-1a2b3c4d.bin", MP));
    TEST_ASSERT_FALSE(r8_accepts("spojboard-matrixportal_s3_2x64-r10-1a2b3c4d.bin", MP));
    TEST_ASSERT_FALSE(r8_accepts("spojboard-esp32_s3_n8r2_2x32-r10-1a2b3c4d.bin", N8));
}

// So an r10 that publishes ONLY display-suffixed assets is invisible to r8
// devices -- they find no match and report "No firmware file found in release".
// A bare alias per board is what keeps them able to update.
void test_r8_needs_a_bare_asset_to_survive_r10(void)
{
    const char* onlyComposites[] = {
        "spojboard-matrixportal_s3_2x32-r10-1a2b3c4d.bin",
        "spojboard-matrixportal_s3_2x64-r10-1a2b3c4d.bin",
    };
    bool any = false;
    for (unsigned k = 0; k < 2; k++)
        any = any || r8_accepts(onlyComposites[k], MP);
    TEST_ASSERT_FALSE(any);

    TEST_ASSERT_TRUE(r8_accepts("spojboard-matrixportal_s3-r10-1a2b3c4d.bin", MP));
}

// r8's parser takes the FIRST "-r", so a display suffix beginning with 'r'
// would truncate the board name and make r8 accept the file. Naming that
// avoids this is a hard constraint on every future release.
void test_r8_r_prefixed_suffix_trap_is_real(void)
{
    TEST_ASSERT_TRUE(r8_accepts("spojboard-matrixportal_s3-rgb-r10-1a2b3c4d.bin", MP));
}

// A dirty local build is still parseable by r8 (it never sees one, but the
// name shape must not become a second grammar).
void test_r8_parses_dirty_build_id(void)
{
    TEST_ASSERT_TRUE(r8_accepts("spojboard-matrixportal_s3-r9-1a2b3c4d-dirty.bin", MP));
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_r8_accepts_r9_bare_assets);
    RUN_TEST(test_r8_still_rejects_the_other_board);
    RUN_TEST(test_r8_ignores_display_suffixed_assets);
    RUN_TEST(test_r8_needs_a_bare_asset_to_survive_r10);
    RUN_TEST(test_r8_r_prefixed_suffix_trap_is_real);
    RUN_TEST(test_r8_parses_dirty_build_id);
    return UNITY_END();
}
