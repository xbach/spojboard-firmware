#ifndef PANEL_GEOMETRY_H
#define PANEL_GEOMETRY_H

#include <stdint.h>

// Panel geometry (TA-0303). Pure: no Arduino, no driver headers, tested in
// test/test_panelgeometry.
//
// Geometry is a RUNTIME setting. It was briefly compiled in (DISPLAY_VARIANT,
// unreleased) on the reasoning that a runtime selector could not express a pin
// map -- but the pin map became a runtime setting too (TA-0302), so the premise
// is gone and the freeze was reverted before it ever shipped.
//
// THE PANEL ARRANGEMENTS, IN FULL. Pixel size does NOT identify the hardware:
// all but the first are 128x64, and they are not interchangeable.
//
//   Chain2x32  128x32   2x 64x32 chained horizontally
//   Grid4x32   128x64   4x 64x32 as a 2x2 serpentine grid
//   Chain2x64  128x64   2x 64x64 chained horizontally
//                       -- OR one single 128x64 module, which is the SAME
//                          thing in code: the driver only ever uses panel
//                          width x chain length as a product, so 64x64 chain 2
//                          and 128x64 chain 1 are indistinguishable to it.
//                          (Verified against the pinned library; see
//                          docs/WIRING.md "A single 128x64 module".)
enum class PanelGeometry : uint8_t
{
    Chain2x32 = 1,
    Grid4x32 = 2,
    Chain2x64 = 3,
};

struct GeometrySpec
{
    // What the HUB75 driver is told.
    int panelWidth;
    int panelHeight;
    int chainLength;
    // A 2x2 grid needs VirtualMatrixPanel to fold the chain; a horizontal chain
    // is already presented as one wide framebuffer and must NOT be remapped.
    bool serpentine;
    // The logical canvas everything else draws on. Differs from the driver's
    // view only for the serpentine grid.
    int displayWidth;
    int displayHeight;
};

GeometrySpec geometrySpec(PanelGeometry geometry);

// Rows of departures this geometry can show: (height / 8) - 1, the last row
// being the status bar. THE single source for the clamp -- three call sites
// used to compute it independently from panelRows.
int geometryMaxDepartureRows(PanelGeometry geometry);

// Whether to drop the HUB75 colour depth from 8 bits to 5. Derived from the
// FRAMEBUFFER, never from a panel count: both 128x64 arrangements allocate an
// identical 64KB, and the ~24KB this frees is what lets the TLS handshake fit
// without any PSRAM tricks.
bool geometryReducedColorDepth(PanelGeometry geometry);

// Migration for devices updating from r9, which stored only `panelRows`.
PanelGeometry geometryFromLegacyPanelRows(int panelRows);

// Derived, and deliberately lossy: panelRows cannot distinguish the two 128x64
// arrangements. Kept so downstream code and the saved config keep their meaning.
int geometryPanelRows(PanelGeometry geometry);

// Short name, sharing the OTA asset grammar's vocabulary ("2x32"/"4x32"/"2x64").
// NO TOKEN MAY BEGIN WITH 'r': r8's parser takes the asset name up to the first
// "-r", so such a token would truncate the board field and make an r8 device
// accept another board's firmware. Pinned by test_panelgeometry and
// test_r8compat.
const char* geometryToken(PanelGeometry geometry);

#endif // PANEL_GEOMETRY_H
