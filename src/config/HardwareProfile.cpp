#include "HardwareProfile.h"

namespace
{
// Which connector position within a triplet feeds red, green and blue, for each
// order. The order's NAME reads out the positions: "GBR" means position 1 is
// wired to the panel's green, position 2 to its blue, position 3 to its red --
// so red is taken from index 2, green from 0, blue from 1.
struct OrderMap
{
    uint8_t r, g, b;
};

const OrderMap ORDER_MAPS[] = {
    {0, 1, 2}, // RGB
    {0, 2, 1}, // RBG
    {1, 0, 2}, // GRB
    {2, 0, 1}, // GBR
    {1, 2, 0}, // BRG
    {2, 1, 0}, // BGR
};

const uint8_t ORDER_COUNT = sizeof(ORDER_MAPS) / sizeof(ORDER_MAPS[0]);
} // namespace

HubPins hwApplyRgbOrder(const HubPins& wired, RgbOrder order)
{
    const uint8_t idx = static_cast<uint8_t>(order);
    // An out-of-range order falls back to RGB rather than indexing past the
    // table: a stored value can be anything once it has survived an NVS
    // migration, and a dead panel is a worse answer than the standard order.
    const OrderMap& m = ORDER_MAPS[idx < ORDER_COUNT ? idx : 0];

    const int8_t top[3] = {wired.r1, wired.g1, wired.b1};
    const int8_t bottom[3] = {wired.r2, wired.g2, wired.b2};

    HubPins out = wired;
    out.r1 = top[m.r];
    out.g1 = top[m.g];
    out.b1 = top[m.b];
    out.r2 = bottom[m.r];
    out.g2 = bottom[m.g];
    out.b2 = bottom[m.b];
    return out;
}

HwPinError hwValidatePins(const HubPins& pins)
{
    const int8_t all[] = {pins.r1, pins.g1, pins.b1, pins.r2, pins.g2, pins.b2,
                          pins.a,  pins.b,  pins.c,  pins.d,  pins.e,
                          pins.lat, pins.oe, pins.clk};
    const uint8_t n = sizeof(all) / sizeof(all[0]);

    for (uint8_t i = 0; i < n; i++)
    {
        const int8_t p = all[i];

        if (p < 0 || p > 48 || (p >= 22 && p <= 25))
        {
            return HwPinError::NotAGpio;
        }
        if (p >= 26 && p <= 32)
        {
            return HwPinError::ReservedForFlash;
        }
        for (uint8_t j = i + 1; j < n; j++)
        {
            if (all[j] == p)
            {
                return HwPinError::Duplicate;
            }
        }
    }

    return HwPinError::None;
}
