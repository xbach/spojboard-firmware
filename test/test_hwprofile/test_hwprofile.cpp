// Runtime HUB75 hardware profile (TA-0302). A wrong pin map is a dead panel on a
// device whose only local output IS the panel, so every rule that decides what
// reaches the driver is pinned here, where a run costs nothing.
#include <unity.h>
#include <string.h>

#include "../../src/config/HardwareProfile.h"
#include "../../src/config/HardwareProfile.cpp"

// The six colour GPIOs as physically wired to HUB75 connector positions 1..6,
// identical on both supported boards.
static HubPins wiredPins()
{
    HubPins p;
    p.r1 = 42; p.g1 = 41; p.b1 = 40;
    p.r2 = 38; p.g2 = 39; p.b2 = 37;
    p.a = 45; p.b = 36; p.c = 48; p.d = 35; p.e = 21;
    p.lat = 47; p.oe = 14; p.clk = 2;
    return p;
}

// ------------------------------------------------------------- channel order

void test_rgb_order_passes_every_pin_through_unchanged(void)
{
    const HubPins in = wiredPins();
    const HubPins out = hwApplyRgbOrder(in, RgbOrder::RGB);

    TEST_ASSERT_EQUAL_INT(42, out.r1);
    TEST_ASSERT_EQUAL_INT(41, out.g1);
    TEST_ASSERT_EQUAL_INT(40, out.b1);
    TEST_ASSERT_EQUAL_INT(38, out.r2);
    TEST_ASSERT_EQUAL_INT(39, out.g2);
    TEST_ASSERT_EQUAL_INT(37, out.b2);
    // address and control lines carry no colour and must never be permuted
    TEST_ASSERT_EQUAL_INT(45, out.a);
    TEST_ASSERT_EQUAL_INT(21, out.e);
    TEST_ASSERT_EQUAL_INT(2, out.clk);
}

// The MatrixPortal's shipped "green/blue reversed" pin map (AppConfig.h:126-136)
// is not a special case -- it is exactly RBG order over the standard wiring.
// If this ever stops holding, the runtime profile cannot reproduce the firmware
// every device in the field is running.
void test_rbg_order_reproduces_the_matrixportal_pin_map(void)
{
    const HubPins out = hwApplyRgbOrder(wiredPins(), RgbOrder::RBG);

    TEST_ASSERT_EQUAL_INT(42, out.r1);
    TEST_ASSERT_EQUAL_INT(40, out.g1);
    TEST_ASSERT_EQUAL_INT(41, out.b1);
    TEST_ASSERT_EQUAL_INT(38, out.r2);
    TEST_ASSERT_EQUAL_INT(37, out.g2);
    TEST_ASSERT_EQUAL_INT(39, out.b2);
}

// The remaining four orders, so the permutation table is pinned in both
// directions rather than only where the two shipped boards happen to sit.
// Wiring is positions 1..6 = {42,41,40,38,39,37}.
void test_remaining_orders_permute_both_triplets(void)
{
    HubPins o = hwApplyRgbOrder(wiredPins(), RgbOrder::GRB);
    TEST_ASSERT_EQUAL_INT(41, o.r1); TEST_ASSERT_EQUAL_INT(42, o.g1); TEST_ASSERT_EQUAL_INT(40, o.b1);
    TEST_ASSERT_EQUAL_INT(39, o.r2); TEST_ASSERT_EQUAL_INT(38, o.g2); TEST_ASSERT_EQUAL_INT(37, o.b2);

    o = hwApplyRgbOrder(wiredPins(), RgbOrder::GBR);
    TEST_ASSERT_EQUAL_INT(40, o.r1); TEST_ASSERT_EQUAL_INT(42, o.g1); TEST_ASSERT_EQUAL_INT(41, o.b1);
    TEST_ASSERT_EQUAL_INT(37, o.r2); TEST_ASSERT_EQUAL_INT(38, o.g2); TEST_ASSERT_EQUAL_INT(39, o.b2);

    o = hwApplyRgbOrder(wiredPins(), RgbOrder::BRG);
    TEST_ASSERT_EQUAL_INT(41, o.r1); TEST_ASSERT_EQUAL_INT(40, o.g1); TEST_ASSERT_EQUAL_INT(42, o.b1);
    TEST_ASSERT_EQUAL_INT(39, o.r2); TEST_ASSERT_EQUAL_INT(37, o.g2); TEST_ASSERT_EQUAL_INT(38, o.b2);

    o = hwApplyRgbOrder(wiredPins(), RgbOrder::BGR);
    TEST_ASSERT_EQUAL_INT(40, o.r1); TEST_ASSERT_EQUAL_INT(41, o.g1); TEST_ASSERT_EQUAL_INT(42, o.b1);
    TEST_ASSERT_EQUAL_INT(37, o.r2); TEST_ASSERT_EQUAL_INT(39, o.g2); TEST_ASSERT_EQUAL_INT(38, o.b2);
}

// ----------------------------------------------------------------- validation

// The most important test in this file. A validator that rejects the wiring
// every shipped device runs is worse than no validator: it fails closed on the
// one map known to work. GPIO 45 is a strapping pin and 35/36/37 are in the
// range an octal-PSRAM part reserves -- so a blocklist copied from a generic
// ESP32-S3 guide would reject reality here.
void test_the_shipped_default_map_validates(void)
{
    TEST_ASSERT_TRUE(hwValidatePins(wiredPins()) == HwPinError::None);
}

void test_duplicate_pin_is_rejected(void)
{
    HubPins p = wiredPins();
    p.b2 = p.r1; // two signals driving one GPIO
    TEST_ASSERT_TRUE(hwValidatePins(p) == HwPinError::Duplicate);
}

// GPIO 26-32 are wired to the SoC's own SPI flash. Driving one does not produce
// a bad picture, it crashes the chip.
void test_flash_pins_are_rejected(void)
{
    HubPins p = wiredPins();
    p.clk = 28;
    TEST_ASSERT_TRUE(hwValidatePins(p) == HwPinError::ReservedForFlash);
}

// The ESP32-S3 has no GPIO 22-25, and none above 48.
void test_nonexistent_gpio_is_rejected(void)
{
    HubPins p = wiredPins();
    p.d = 24;
    TEST_ASSERT_TRUE(hwValidatePins(p) == HwPinError::NotAGpio);

    HubPins q = wiredPins();
    q.d = 49;
    TEST_ASSERT_TRUE(hwValidatePins(q) == HwPinError::NotAGpio);
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_rgb_order_passes_every_pin_through_unchanged);
    RUN_TEST(test_rbg_order_reproduces_the_matrixportal_pin_map);
    RUN_TEST(test_remaining_orders_permute_both_triplets);
    RUN_TEST(test_the_shipped_default_map_validates);
    RUN_TEST(test_duplicate_pin_is_rejected);
    RUN_TEST(test_flash_pins_are_rejected);
    RUN_TEST(test_nonexistent_gpio_is_rejected);
    return UNITY_END();
}
