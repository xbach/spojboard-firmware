#include "PanelGeometry.h"

GeometrySpec geometrySpec(PanelGeometry geometry)
{
    GeometrySpec s;
    s.panelWidth = 64;

    switch (geometry)
    {
    case PanelGeometry::Grid4x32:
        s.panelHeight = 32;
        s.chainLength = 4;
        s.serpentine = true;
        s.displayWidth = 128;
        s.displayHeight = 64;
        break;

    case PanelGeometry::Chain2x64:
        s.panelHeight = 64;
        s.chainLength = 2;
        s.serpentine = false;
        s.displayWidth = 128;
        s.displayHeight = 64;
        break;

    case PanelGeometry::Chain2x32:
    default:
        // Unknown values fall back to the geometry every release has shipped.
        s.panelHeight = 32;
        s.chainLength = 2;
        s.serpentine = false;
        s.displayWidth = 128;
        s.displayHeight = 32;
        break;
    }

    return s;
}

int geometryMaxDepartureRows(PanelGeometry geometry)
{
    const GeometrySpec s = geometrySpec(geometry);
    return (s.displayHeight / 8) - 1;
}

bool geometryReducedColorDepth(PanelGeometry geometry)
{
    const GeometrySpec s = geometrySpec(geometry);
    return (s.displayWidth * s.displayHeight) > (128 * 32);
}

PanelGeometry geometryFromLegacyPanelRows(int panelRows)
{
    // 2 has always been labelled "128x64 (4 panels)" in the UI, so it means the
    // serpentine grid. Anything else is absent or corrupt: fall back to the
    // geometry every release has shipped.
    return (panelRows == 2) ? PanelGeometry::Grid4x32 : PanelGeometry::Chain2x32;
}

int geometryPanelRows(PanelGeometry geometry)
{
    return geometrySpec(geometry).displayHeight / 32;
}

const char* geometryToken(PanelGeometry geometry)
{
    switch (geometry)
    {
    case PanelGeometry::Grid4x32:
        return "4x32";
    case PanelGeometry::Chain2x64:
        return "2x64";
    case PanelGeometry::Chain2x32:
    default:
        return "2x32";
    }
}
