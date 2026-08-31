#ifndef HARDWARE_PROFILE_H
#define HARDWARE_PROFILE_H

#include <stdint.h>

// Runtime HUB75 hardware profile (TA-0302).
//
// Pure C++: no Arduino, no driver headers. Everything here is decided on the
// desktop and tested in test/test_hwprofile, because the failure mode on device
// is a dead panel and the panel is the only local output there is.

// The 14 GPIOs the HUB75 driver takes, in its own naming.
struct HubPins
{
    int8_t r1, g1, b1, r2, g2, b2;
    int8_t a, b, c, d, e;
    int8_t lat, oe, clk;
};

// Which colour each of the two RGB triplets carries. The pins are named for
// connector POSITION as wired; the order says what the panel expects to find
// there. A panel with green and blue transposed is not a rendering problem --
// it is this permutation.
enum class RgbOrder : uint8_t
{
    RGB = 0,
    RBG,
    GRB,
    GBR,
    BRG,
    BGR
};

// Why a pin cannot be used. Deliberately SHORT: the two boards this firmware
// supports drive GPIO 45 (a strapping pin) and 35/36/37 (reserved only on
// octal-PSRAM parts, which neither board is), so a blocklist lifted from a
// generic ESP32-S3 guide would reject the exact wiring every shipped device
// runs. Only rules that are fatal on OUR parts are enforced here.
enum class HwPinError : uint8_t
{
    None = 0,
    Duplicate,        // two signals driving one GPIO
    NotAGpio,         // no such pin on an ESP32-S3 (22-25 absent, none above 48)
    ReservedForFlash  // 26-32 belong to the SoC's SPI flash; driving one crashes it
};

// Check a pin map before anything reaches the driver. Order does not matter to
// validity, so validate the wired map, not the permuted one.
HwPinError hwValidatePins(const HubPins& pins);

// Relabel the six colour pins for `order`. Address and control lines carry no
// colour and always pass through untouched.
HubPins hwApplyRgbOrder(const HubPins& wired, RgbOrder order);

// A device's stored display wiring. `pins` is always in connector order; the
// order is applied on top, so a stock-wired panel with transposed channels
// needs no pin edits at all.
struct HwProfile
{
    bool useCustomPins;
    HubPins pins;
    RgbOrder order;
};

// The map that reaches the driver. Falls back to `compiledDefault` when the
// stored map is absent or fails validation -- NVS survives OTA and migrations,
// so a stored map can be anything by the time it is read, and the driver must
// never receive one that validation rejects.
HubPins hwResolvePins(const HwProfile& profile, const HubPins& compiledDefault);

#endif // HARDWARE_PROFILE_H
