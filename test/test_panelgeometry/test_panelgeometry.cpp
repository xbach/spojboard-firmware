// Panel geometry (TA-0303). Every number the display layer derives from "what
// panels are attached" comes from one table here, so the three clamp sites that
// used to compute (panelRows * 32 / 8) - 1 independently cannot drift apart.
#include <unity.h>

#include "../../src/config/PanelGeometry.h"
#include "../../src/config/PanelGeometry.cpp"

// -------------------------------------------------------------- driver config

void test_2x32_is_two_64x32_panels_chained(void)
{
    const GeometrySpec s = geometrySpec(PanelGeometry::Chain2x32);
    TEST_ASSERT_EQUAL_INT(64, s.panelWidth);
    TEST_ASSERT_EQUAL_INT(32, s.panelHeight);
    TEST_ASSERT_EQUAL_INT(2, s.chainLength);
    TEST_ASSERT_FALSE(s.serpentine);
    TEST_ASSERT_EQUAL_INT(128, s.displayWidth);
    TEST_ASSERT_EQUAL_INT(32, s.displayHeight);
}

// The 2x2 grid is the only arrangement whose logical canvas differs from what
// the DMA driver is told: the driver sees a 256x32 chain and VirtualMatrixPanel
// folds it into 128x64.
void test_4x32_is_a_serpentine_grid_folded_to_128x64(void)
{
    const GeometrySpec s = geometrySpec(PanelGeometry::Grid4x32);
    TEST_ASSERT_EQUAL_INT(64, s.panelWidth);
    TEST_ASSERT_EQUAL_INT(32, s.panelHeight);
    TEST_ASSERT_EQUAL_INT(4, s.chainLength);
    TEST_ASSERT_TRUE(s.serpentine);
    TEST_ASSERT_EQUAL_INT(128, s.displayWidth);
    TEST_ASSERT_EQUAL_INT(64, s.displayHeight);
}

void test_2x64_is_a_plain_chain_of_64_high_panels(void)
{
    const GeometrySpec s = geometrySpec(PanelGeometry::Chain2x64);
    TEST_ASSERT_EQUAL_INT(64, s.panelHeight);
    TEST_ASSERT_EQUAL_INT(2, s.chainLength);
    TEST_ASSERT_FALSE(s.serpentine);
    TEST_ASSERT_EQUAL_INT(128, s.displayWidth);
    TEST_ASSERT_EQUAL_INT(64, s.displayHeight);
}

// ------------------------------------------------------- derived layout facts

void test_departure_row_count_follows_panel_height(void)
{
    TEST_ASSERT_EQUAL_INT(3, geometryMaxDepartureRows(PanelGeometry::Chain2x32));
    TEST_ASSERT_EQUAL_INT(7, geometryMaxDepartureRows(PanelGeometry::Grid4x32));
    TEST_ASSERT_EQUAL_INT(7, geometryMaxDepartureRows(PanelGeometry::Chain2x64));
}

// Colour depth is a function of FRAMEBUFFER SIZE, never of panel count. A
// `panelCount > 2` test was once used as a proxy for it and inverted silently
// the day 2x64 arrived -- 128x64, therefore 5-bit, but only two panels. Both
// 128x64 arrangements allocate an identical 64KB and must answer the same.
void test_reduced_color_depth_tracks_pixels_not_panels(void)
{
    TEST_ASSERT_FALSE(geometryReducedColorDepth(PanelGeometry::Chain2x32));
    TEST_ASSERT_TRUE(geometryReducedColorDepth(PanelGeometry::Grid4x32));
    TEST_ASSERT_TRUE(geometryReducedColorDepth(PanelGeometry::Chain2x64));
}

// ------------------------------------------------------------------ migration

// Devices updating from r9 have only the old panelRows int. The mapping is
// unambiguous because the UI has always labelled the 2 option "128x64 (4
// panels)" -- so a stored 2 means the serpentine grid, never 2x 64x64. An owner
// of 64x64 panels never had a setting that could express their hardware and
// picks it after updating.
void test_legacy_panel_rows_migrate_to_a_named_arrangement(void)
{
    TEST_ASSERT_TRUE(geometryFromLegacyPanelRows(1) == PanelGeometry::Chain2x32);
    TEST_ASSERT_TRUE(geometryFromLegacyPanelRows(2) == PanelGeometry::Grid4x32);
    // Anything else is a corrupt or absent key: fail to the shipped geometry.
    TEST_ASSERT_TRUE(geometryFromLegacyPanelRows(0) == PanelGeometry::Chain2x32);
    TEST_ASSERT_TRUE(geometryFromLegacyPanelRows(99) == PanelGeometry::Chain2x32);
}

// panelRows survives as a DERIVED value so downstream code and the saved config
// keep meaning what they meant, but it is no longer the source of truth: it
// cannot tell 4x32 from 2x64.
void test_panel_rows_is_derived_and_lossy(void)
{
    TEST_ASSERT_EQUAL_INT(1, geometryPanelRows(PanelGeometry::Chain2x32));
    TEST_ASSERT_EQUAL_INT(2, geometryPanelRows(PanelGeometry::Grid4x32));
    TEST_ASSERT_EQUAL_INT(2, geometryPanelRows(PanelGeometry::Chain2x64));
}

// The token is the same vocabulary the OTA asset grammar uses, so it must keep
// matching it exactly -- and no token may begin with 'r', which would make an
// r8 device mis-parse a filename and accept another board's firmware.
void test_tokens_match_the_ota_asset_vocabulary(void)
{
    TEST_ASSERT_EQUAL_STRING("2x32", geometryToken(PanelGeometry::Chain2x32));
    TEST_ASSERT_EQUAL_STRING("4x32", geometryToken(PanelGeometry::Grid4x32));
    TEST_ASSERT_EQUAL_STRING("2x64", geometryToken(PanelGeometry::Chain2x64));

    TEST_ASSERT_NOT_EQUAL('r', geometryToken(PanelGeometry::Chain2x32)[0]);
    TEST_ASSERT_NOT_EQUAL('r', geometryToken(PanelGeometry::Grid4x32)[0]);
    TEST_ASSERT_NOT_EQUAL('r', geometryToken(PanelGeometry::Chain2x64)[0]);
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_2x32_is_two_64x32_panels_chained);
    RUN_TEST(test_4x32_is_a_serpentine_grid_folded_to_128x64);
    RUN_TEST(test_2x64_is_a_plain_chain_of_64_high_panels);
    RUN_TEST(test_departure_row_count_follows_panel_height);
    RUN_TEST(test_reduced_color_depth_tracks_pixels_not_panels);
    RUN_TEST(test_legacy_panel_rows_migrate_to_a_named_arrangement);
    RUN_TEST(test_panel_rows_is_derived_and_lossy);
    RUN_TEST(test_tokens_match_the_ota_asset_vocabulary);
    return UNITY_END();
}
